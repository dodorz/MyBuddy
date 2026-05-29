#include "app.h"

#include "config.h"
#include "state.h"
#include "version.h"

#include <algorithm>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>

namespace {
constexpr UINT kTrayMsg = WM_APP + 1;
constexpr UINT kTrayIconId = 1001;
constexpr wchar_t kMainClass[] = L"MyBuddyMainClass";
constexpr wchar_t kHotZoneClass[] = L"MyBuddyHotZoneClass";

std::wstring EnsureTrailingSlash(std::wstring path) {
  if (!path.empty() && path.back() != L'\\') path.push_back(L'\\');
  return path;
}

void AddTrayIcon(HWND hwnd) {
  NOTIFYICONDATAW nid{};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd;
  nid.uID = kTrayIconId;
  nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  nid.uCallbackMessage = kTrayMsg;
  nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wcscpy_s(nid.szTip, L"MyBuddy");
  Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hwnd) {
  NOTIFYICONDATAW nid{};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd;
  nid.uID = kTrayIconId;
  Shell_NotifyIconW(NIM_DELETE, &nid);
}

}

int App::Run(HINSTANCE instance, int showCmd) {
  instance_ = instance;

  WNDCLASSW mainClass{};
  mainClass.lpfnWndProc = App::MainWndProc;
  mainClass.hInstance = instance_;
  mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  mainClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  mainClass.lpszClassName = kMainClass;
  mainClass.cbWndExtra = sizeof(LONG_PTR);
  RegisterClassW(&mainClass);

  WNDCLASSW hotClass{};
  hotClass.lpfnWndProc = App::HotZoneWndProc;
  hotClass.hInstance = instance_;
  hotClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  hotClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  hotClass.lpszClassName = kHotZoneClass;
  hotClass.cbWndExtra = sizeof(LONG_PTR);
  RegisterClassW(&hotClass);

  std::wstring title = L"MyBuddy ";
  title += MYBUDDY_VERSION_STRING;

  hwnd_ = CreateWindowExW(
    WS_EX_APPWINDOW,
    kMainClass,
    title.c_str(),
    WS_POPUP | WS_THICKFRAME,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    state_.w,
    state_.h,
    nullptr,
    nullptr,
    instance_,
    this);
  if (!hwnd_) return 0;

  AddTrayIcon(hwnd_);
  ShowWindow(hwnd_, showCmd);
  UpdateWindow(hwnd_);

  MSG msg{};
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK App::MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
    self = reinterpret_cast<App*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  }
  return self ? self->HandleMainMessage(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK App::HotZoneWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
    self = reinterpret_cast<App*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hotZone_ = hwnd;
  }
  return self ? self->HandleHotZoneMessage(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT App::HandleMainMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CREATE: {
      LoadConfig();
      LoadState();
      status_ = CreateWindowExW(0, L"STATIC", L"MyBuddy ready", WS_CHILD | WS_VISIBLE,
                                16, 16, 280, 24, hwnd, nullptr, instance_, nullptr);
      CreateWindowExW(0, L"BUTTON", L"Demo Action", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      16, 52, 120, 28, hwnd, reinterpret_cast<HMENU>(1), instance_, nullptr);
      ApplySavedGeometry();
      ShowWindow(hwnd, state_.expanded ? SW_SHOWNA : SW_SHOWNOACTIVATE);
      if (!state_.expanded && config_.autoHide) {
        ShowMainWindow();
        RequestCollapse();
      } else {
        ShowMainWindow();
        ShowHotZone(false);
      }
      return 0;
    }
    case WM_COMMAND:
      if (LOWORD(wp) == 1) {
        MessageBoxW(hwnd, L"Placeholder action", L"MyBuddy", MB_OK | MB_ICONINFORMATION);
      }
      return 0;
    case WM_SIZE:
      if (status_) {
        MoveWindow(status_, 16, 16, 280, 24, TRUE);
      }
      return 0;
    case WM_MOUSEMOVE:
      if (!trackingLeave_) {
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        trackingLeave_ = true;
      }
      if (config_.autoHide) {
        DisarmCollapseTimer();
      }
      return 0;
    case WM_MOUSELEAVE:
      trackingLeave_ = false;
      if (config_.autoHide && state_.expanded) {
        ArmCollapseTimer();
      }
      return 0;
    case WM_TIMER:
      if (wp == collapseTimer_) {
        if (config_.autoHide && state_.expanded && !IsPointerInsideMain() && !IsPointerInsideHotZone()) {
          RequestCollapse();
        }
        return 0;
      }
      if (wp == animationTimer_) {
        TickAnimation();
        return 0;
      }
      return 0;
    case WM_EXITSIZEMOVE:
    case WM_MOVE: {
      if (!animation_.active) {
        RECT r{};
        GetWindowRect(hwnd, &r);
        currentRect_ = r;
        SetDockEdgeFromRect(r);
        state_.x = r.left;
        state_.y = r.top;
        state_.w = r.right - r.left;
        state_.h = r.bottom - r.top;
        SaveState();
        UpdateHotZonePlacement();
      }
      return 0;
    }
    case WM_CLOSE:
      closing_ = true;
      DestroyHotZoneWindow();
      RemoveTrayIcon(hwnd);
      SaveState();
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      if (msg == kTrayMsg) {
        if (LOWORD(lp) == WM_LBUTTONUP || LOWORD(lp) == WM_LBUTTONDBLCLK) {
          if (IsWindowVisible(hwnd)) {
            RequestExpand();
          } else {
            ShowMainWindow();
            RequestExpand();
          }
        }
        return 0;
      }
      break;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT App::HandleHotZoneMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CREATE:
      SetLayeredWindowAttributes(hwnd, 0, 1, LWA_ALPHA);
      return 0;
    case WM_MOUSEMOVE:
      RequestExpand();
      return 0;
    case WM_MOUSELEAVE:
      if (config_.autoHide && state_.expanded) {
        ArmCollapseTimer();
      }
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

bool App::LoadConfig() {
  LoadAppConfig(config_, GetProgramConfigPath(), GetFallbackConfigPath());
  return true;
}

bool App::LoadState() {
  return LoadAppState(state_, GetStatePath());
}

void App::SaveState() const {
  SaveAppState(state_, GetStatePath());
}

void App::CreateHotZoneWindow() {
  if (hotZone_) return;
  hotZone_ = CreateWindowExW(
    WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE,
    kHotZoneClass,
    L"",
    WS_POPUP,
    0, 0, 1, 1,
    nullptr,
    nullptr,
    instance_,
    this);
}

void App::DestroyHotZoneWindow() {
  if (hotZone_) {
    DestroyWindow(hotZone_);
    hotZone_ = nullptr;
  }
}

RECT App::GetWorkArea() const {
  MONITORINFO mi{ sizeof(mi) };
  GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
  return mi.rcWork;
}

void App::SetDockEdgeFromRect(const RECT& rc) {
  RECT work = GetWorkArea();
  int leftDist = abs(rc.left - work.left);
  int rightDist = abs(work.right - rc.right);
  int topDist = abs(rc.top - work.top);
  int bottomDist = abs(work.bottom - rc.bottom);
  if (leftDist <= rightDist && leftDist <= topDist && leftDist <= bottomDist) {
    state_.dockEdge = 0;
  } else if (rightDist <= topDist && rightDist <= bottomDist) {
    state_.dockEdge = 1;
  } else if (topDist <= bottomDist) {
    state_.dockEdge = 2;
  } else {
    state_.dockEdge = 3;
  }
}

RECT App::GetTargetRect(bool expanded) const {
  RECT work = GetWorkArea();
  int fullW = std::max(280, state_.w);
  int fullH = std::max(260, state_.h);
  int peek = std::max(4, config_.edgePeekPx);
  int x = state_.x;
  int y = state_.y;

  switch (static_cast<DockEdge>(state_.dockEdge)) {
    case DockEdge::Left:
      x = expanded ? work.left : work.left - (fullW - peek);
      y = work.top + 40;
      return RECT{ x, y, x + (expanded ? fullW : peek), y + fullH };
    case DockEdge::Right:
      x = expanded ? work.right - fullW : work.right - peek;
      y = work.top + 40;
      return RECT{ x, y, x + (expanded ? fullW : peek), y + fullH };
    case DockEdge::Top:
      x = work.left + 40;
      y = expanded ? work.top : work.top - (fullH - peek);
      return RECT{ x, y, x + fullW, y + (expanded ? fullH : peek) };
    case DockEdge::Bottom:
    default:
      x = work.left + 40;
      y = expanded ? work.bottom - fullH : work.bottom - peek;
      return RECT{ x, y, x + fullW, y + (expanded ? fullH : peek) };
  }
}

RECT App::GetHiddenRect() const {
  return GetTargetRect(false);
}

void App::UpdateHotZonePlacement() {
  if (!hotZone_) {
    CreateHotZoneWindow();
  }
  if (!hotZone_) return;

  RECT work = GetWorkArea();
  int peek = std::max(4, config_.edgePeekPx);
  RECT r{};
  switch (static_cast<DockEdge>(state_.dockEdge)) {
    case DockEdge::Left:
      r = RECT{ work.left, work.top, work.left + peek, work.bottom };
      break;
    case DockEdge::Right:
      r = RECT{ work.right - peek, work.top, work.right, work.bottom };
      break;
    case DockEdge::Top:
      r = RECT{ work.left, work.top, work.right, work.top + peek };
      break;
    case DockEdge::Bottom:
    default:
      r = RECT{ work.left, work.bottom - peek, work.right, work.bottom };
      break;
  }

  SetWindowPos(
    hotZone_,
    HWND_TOPMOST,
    r.left,
    r.top,
    r.right - r.left,
    r.bottom - r.top,
    SWP_NOACTIVATE | SWP_SHOWWINDOW);
  ShowHotZone(!state_.expanded && config_.autoHide);
}

void App::ShowHotZone(bool show) {
  if (!hotZone_) return;
  hotZoneVisible_ = show;
  ShowWindow(hotZone_, show ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void App::ShowMainWindow() {
  ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
  UpdateWindow(hwnd_);
}

void App::HideMainWindow() {
  ShowWindow(hwnd_, SW_HIDE);
}

void App::ApplySavedGeometry() {
  if (state_.version != 1) {
    state_.version = 1;
  }
  if (state_.w <= 0 || state_.h <= 0) {
    state_.w = 360;
    state_.h = 520;
  }
  RECT target = state_.expanded ? GetTargetRect(true) : GetTargetRect(false);
  currentRect_ = target;
  SetWindowPos(
    hwnd_,
    HWND_TOPMOST,
    target.left,
    target.top,
    target.right - target.left,
    target.bottom - target.top,
    SWP_NOACTIVATE | SWP_NOZORDER);
  UpdateHotZonePlacement();
}

void App::RequestExpand() {
  DisarmCollapseTimer();
  ShowMainWindow();
  if (!state_.expanded) {
    BeginAnimation(true);
  } else {
    UpdateHotZonePlacement();
  }
}

void App::RequestCollapse() {
  if (!config_.autoHide || closing_) return;
  if (animation_.active || !state_.expanded) return;
  BeginAnimation(false);
}

void App::BeginAnimation(bool expanded) {
  animation_.active = true;
  animation_.targetExpanded = expanded;
  animation_.from = currentRect_;
  animation_.to = expanded ? GetTargetRect(true) : GetTargetRect(false);
  animation_.startTick = GetTickCount64();
  SetTimer(hwnd_, animationTimer_, 16, nullptr);
}

void App::TickAnimation() {
  if (!animation_.active) {
    KillTimer(hwnd_, animationTimer_);
    return;
  }

  ULONGLONG now = GetTickCount64();
  int duration = std::max(1, config_.slideMs);
  double t = std::clamp(static_cast<double>(now - animation_.startTick) / duration, 0.0, 1.0);
  double eased = 1.0 - (1.0 - t) * (1.0 - t);

  auto lerp = [eased](int a, int b) {
    return static_cast<int>(a + (b - a) * eased);
  };

  RECT r{};
  r.left = lerp(animation_.from.left, animation_.to.left);
  r.top = lerp(animation_.from.top, animation_.to.top);
  r.right = lerp(animation_.from.right, animation_.to.right);
  r.bottom = lerp(animation_.from.bottom, animation_.to.bottom);

  SetWindowPos(
    hwnd_,
    HWND_TOPMOST,
    r.left,
    r.top,
    r.right - r.left,
    r.bottom - r.top,
    SWP_NOACTIVATE | SWP_NOZORDER);

  currentRect_ = r;
  if (t >= 1.0) {
    FinishAnimation();
  }
}

void App::FinishAnimation() {
  KillTimer(hwnd_, animationTimer_);
  animation_.active = false;
  currentRect_ = animation_.to;
  state_.expanded = animation_.targetExpanded;
  state_.x = currentRect_.left;
  state_.y = currentRect_.top;
  state_.w = currentRect_.right - currentRect_.left;
  state_.h = currentRect_.bottom - currentRect_.top;
  if (state_.expanded) {
    ShowHotZone(false);
  } else {
    UpdateHotZonePlacement();
  }
  SaveState();
}

void App::ArmCollapseTimer() {
  if (!collapseTimer_) {
    collapseTimer_ = 1;
  }
  SetTimer(hwnd_, collapseTimer_, 120, nullptr);
}

void App::DisarmCollapseTimer() {
  KillTimer(hwnd_, collapseTimer_);
}

bool App::IsPointerInsideMain() const {
  POINT pt{};
  GetCursorPos(&pt);
  RECT r{};
  GetWindowRect(hwnd_, &r);
  return PtInRect(&r, pt);
}

bool App::IsPointerInsideHotZone() const {
  if (!hotZone_ || !IsWindowVisible(hotZone_)) return false;
  POINT pt{};
  GetCursorPos(&pt);
  RECT r{};
  GetWindowRect(hotZone_, &r);
  return PtInRect(&r, pt);
}

std::wstring App::GetAppDataDir() const {
  PWSTR knownPath = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &knownPath))) {
    return L".\\";
  }
  std::wstring dir = EnsureTrailingSlash(knownPath);
  CoTaskMemFree(knownPath);
  dir += config_.appName;
  CreateDirectoryW(dir.c_str(), nullptr);
  return EnsureTrailingSlash(dir);
}

std::wstring App::GetProgramConfigPath() const {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring exe = path;
  auto pos = exe.find_last_of(L"\\/");
  return exe.substr(0, pos + 1) + L"config.ini";
}

std::wstring App::GetFallbackConfigPath() const {
  return GetAppDataDir() + L"config.ini";
}

std::wstring App::GetStatePath() const {
  return GetAppDataDir() + L"state.dat";
}
