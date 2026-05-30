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
constexpr UINT kTrayOpenCmd = 2001;
constexpr UINT kTrayExitCmd = 2002;
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

void UpdateTrayIconTip(HWND hwnd, const wchar_t* tip) {
  NOTIFYICONDATAW nid{};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd;
  nid.uID = kTrayIconId;
  nid.uFlags = NIF_TIP;
  wcscpy_s(nid.szTip, tip);
  Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void ShowTrayContextMenu(HWND hwnd, POINT pt) {
  HMENU menu = CreatePopupMenu();
  if (!menu) return;
  AppendMenuW(menu, MF_STRING, kTrayOpenCmd, L"Open");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kTrayExitCmd, L"Exit");
  SetForegroundWindow(hwnd);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd, nullptr);
  PostMessageW(hwnd, WM_NULL, 0, 0);
  DestroyMenu(menu);
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
    WS_EX_APPWINDOW | WS_EX_TOPMOST,
    kMainClass,
    title.c_str(),
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX,
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
  UpdateTrayIconTip(hwnd_, title.c_str());
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
      stateLoaded_ = LoadState();
      if (!stateLoaded_) {
        InitializeDefaultState();
      }
      state_.expanded = true;
      taskbarVisible_ = state_.taskbarVisible;
      if (!stateLoaded_) {
        taskbarVisible_ = true;
        state_.taskbarVisible = true;
      }
      status_ = CreateWindowExW(0, L"STATIC", L"MyBuddy ready", WS_CHILD | WS_VISIBLE,
                                16, 16, 280, 24, hwnd, nullptr, instance_, nullptr);
      CreateWindowExW(0, L"BUTTON", L"Demo Action", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      16, 52, 120, 28, hwnd, reinterpret_cast<HMENU>(1), instance_, nullptr);
      ApplySavedGeometry();
      ShowMainWindow();
      ShowHotZone(false);
      return 0;
    }
    case WM_COMMAND:
      if (LOWORD(wp) == 1) {
        MessageBoxW(hwnd, L"Placeholder action", L"MyBuddy", MB_OK | MB_ICONINFORMATION);
      } else if (LOWORD(wp) == kTrayOpenCmd) {
        ShowMainWindow();
        RequestExpand();
      } else if (LOWORD(wp) == kTrayExitCmd) {
        ExitFromTray();
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
      DisarmCollapseTimer();
      return 0;
    case WM_MOUSELEAVE:
      trackingLeave_ = false;
      if (state_.expanded && !animation_.active) {
        ArmCollapseTimer();
      }
      return 0;
    case WM_ENTERSIZEMOVE:
      DisarmCollapseTimer();
      return 0;
    case WM_TIMER:
      if (wp == animationTimer_) {
        TickAnimation();
        return 0;
      }
      if (wp == collapseTimer_) {
        DisarmCollapseTimer();
        if (!IsPointerInsideMain() && state_.expanded && !animation_.active) {
          RequestCollapse();
        }
        return 0;
      }
      if (wp == hotZoneTimer_) {
        if (hotZoneVisible_ && !animation_.active && IsPointerInsideHotZone()) {
          ShowMainWindow();
          ShowHotZone(false);
          BeginAnimation(true);
        }
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
      ShowToTray();
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
        } else if (LOWORD(lp) == WM_RBUTTONUP) {
          POINT pt{};
          GetCursorPos(&pt);
          ShowTrayMenu(pt);
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
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_MOUSEMOVE: {
      TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
      TrackMouseEvent(&tme);
      UpdateHotZoneTrigger();
      return 0;
    }
    case WM_MOUSELEAVE:
      UpdateHotZoneTrigger();
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

void App::SetTaskbarVisible(bool visible) {
  taskbarVisible_ = visible;
  state_.taskbarVisible = visible;
  LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
  ex &= ~(WS_EX_APPWINDOW | WS_EX_TOOLWINDOW);
  if (visible) {
    ex |= WS_EX_APPWINDOW;
  } else {
    ex |= WS_EX_TOOLWINDOW;
  }
  SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex);
  SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void App::ShowToTray() {
  closing_ = false;
  state_.expanded = false;
  SetTaskbarVisible(false);
  HideMainWindow();
  ShowHotZone(false);
  SaveState();
}

void App::ExitFromTray() {
  closing_ = true;
  DestroyHotZoneWindow();
  RemoveTrayIcon(hwnd_);
  SaveState();
  DestroyWindow(hwnd_);
}

void App::ShowTrayMenu(POINT pt) {
  SetForegroundWindow(hwnd_);
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING, kTrayOpenCmd, L"Open");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kTrayExitCmd, L"Exit");
  TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);
}

void App::InitializeDefaultState() {
  RECT work = GetWorkArea();
  state_.dockEdge = 1;
  state_.w = 360;
  state_.h = 520;
  state_.x = work.right - state_.w - 40;
  state_.y = work.top + 80;
  state_.expanded = true;
  state_.taskbarVisible = false;
}

void App::CreateHotZoneWindow() {
  if (hotZone_) return;
  hotZone_ = CreateWindowExW(
    WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED,
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
  int minDist = std::min(std::min(leftDist, rightDist), std::min(topDist, bottomDist));
  if (topDist == minDist) {
    state_.dockEdge = 2;
  } else if (rightDist == minDist) {
    state_.dockEdge = 1;
  } else if (leftDist == minDist) {
    state_.dockEdge = 0;
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
      x = std::max<int>(work.left, std::min<int>(state_.x, work.right - fullW));
      y = expanded ? work.top : work.top - (fullH - peek);
      return RECT{ x, y, x + fullW, y + (expanded ? fullH : peek) };
    case DockEdge::Bottom:
    default:
      x = std::max<int>(work.left, std::min<int>(state_.x, work.right - fullW));
      y = expanded ? work.bottom - fullH : work.bottom - peek;
      return RECT{ x, y, x + fullW, y + (expanded ? fullH : peek) };
  }
}

RECT App::GetHiddenRect() const {
  return GetTargetRect(true);
}

RECT App::GetHotZoneRect() const {
  RECT work = GetWorkArea();
  int peek = std::max(4, config_.edgePeekPx);
  switch (static_cast<DockEdge>(state_.dockEdge)) {
    case DockEdge::Left:
      return RECT{ work.left, work.top, work.left + peek, work.bottom };
    case DockEdge::Right:
      return RECT{ work.right - peek, work.top, work.right, work.bottom };
    case DockEdge::Top:
      return RECT{ work.left, work.top, work.right, work.top + peek };
    case DockEdge::Bottom:
    default:
      return RECT{ work.left, work.bottom - peek, work.right, work.bottom };
  }
}

void App::UpdateHotZonePlacement() {
  if (!state_.expanded && !animation_.active) {
    if (!hotZone_) CreateHotZoneWindow();
    ShowHotZone(true);
  } else {
    ShowHotZone(false);
  }
}

void App::ShowHotZone(bool show) {
  hotZoneVisible_ = show;
  if (!hotZone_) return;
  if (show) {
    RECT r = GetHotZoneRect();
    SetWindowPos(hotZone_, HWND_TOPMOST, r.left, r.top,
      r.right - r.left, r.bottom - r.top,
      SWP_SHOWWINDOW | SWP_NOACTIVATE);
    SetTimer(hwnd_, hotZoneTimer_, 80, nullptr);
  } else {
    ShowWindow(hotZone_, SW_HIDE);
    KillTimer(hwnd_, hotZoneTimer_);
  }
}

void App::UpdateHotZoneTrigger() {
  if (!hotZoneVisible_ || animation_.active) return;
  if (IsPointerInsideHotZone()) {
    ShowMainWindow();
    ShowHotZone(false);
    BeginAnimation(true);
  }
}

void App::ShowMainWindow() {
  ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
  UpdateWindow(hwnd_);
}

void App::HideMainWindow() {
  ShowWindow(hwnd_, SW_HIDE);
}

void App::ApplySavedGeometry() {
  if (state_.version != 2) {
    state_.version = 2;
  }
  if (state_.w <= 0 || state_.h <= 0) {
    state_.w = 360;
    state_.h = 520;
  }
  RECT work = GetWorkArea();
  if (!stateLoaded_) {
    int minX = work.left + 20;
    int maxX = work.right - state_.w - 20;
    int minY = work.top + 20;
    int maxY = work.bottom - state_.h - 20;
    if (maxX < minX) maxX = minX;
    if (maxY < minY) maxY = minY;
    state_.x = std::max(minX, std::min(state_.x, maxX));
    state_.y = std::max(minY, std::min(state_.y, maxY));
  }
  if (startupRestoreExpanded_ && state_.expanded) {
    RECT work = GetWorkArea();
    int fullW = std::max(280, state_.w);
    int fullH = std::max(260, state_.h);
    state_.dockEdge = std::clamp(state_.dockEdge, 0, 3);
    switch (static_cast<DockEdge>(state_.dockEdge)) {
      case DockEdge::Left:
        state_.x = work.left + 20;
        state_.y = work.top + 80;
        break;
      case DockEdge::Right:
        state_.x = std::max(work.left + 20, work.right - fullW - 20);
        state_.y = work.top + 80;
        break;
      case DockEdge::Top:
        state_.x = work.left + 80;
        state_.y = work.top + 20;
        break;
      case DockEdge::Bottom:
      default:
        state_.x = work.left + 80;
        state_.y = std::max(work.top + 20, work.bottom - fullH - 20);
        break;
    }
  }

  RECT target = GetTargetRect(true);
  currentRect_ = target;
  SetTaskbarVisible(state_.taskbarVisible);
  SetWindowPos(
    hwnd_,
    HWND_TOPMOST,
    target.left,
    target.top,
    target.right - target.left,
    target.bottom - target.top,
    SWP_NOACTIVATE | SWP_NOZORDER);
  if (state_.expanded) {
    SetForegroundWindow(hwnd_);
  }
  UpdateHotZonePlacement();
}

void App::RequestExpand() {
  ShowMainWindow();
  SetTaskbarVisible(true);
  state_.expanded = true;
  RECT target = GetTargetRect(true);
  currentRect_ = target;
  SetWindowPos(hwnd_, HWND_TOPMOST, target.left, target.top,
    target.right - target.left, target.bottom - target.top,
    SWP_NOACTIVATE | SWP_NOZORDER);
  SaveState();
}

void App::RequestCollapse() {
  if (!state_.expanded || animation_.active) return;
  DisarmCollapseTimer();
  BeginAnimation(false);
}

void App::BeginAnimation(bool expanded) {
  animation_.active = true;
  animation_.targetExpanded = expanded;
  animation_.from = currentRect_;
  animation_.to = expanded ? GetTargetRect(true) : GetTargetRect(false);
  animation_.startTick = GetTickCount64();
  animation_.durationMs = expanded ? std::max(80, config_.slideMs - 80) : std::max(150, config_.slideMs);
  SetTimer(hwnd_, animationTimer_, 16, nullptr);
}

void App::TickAnimation() {
  if (!animation_.active) {
    KillTimer(hwnd_, animationTimer_);
    return;
  }

  ULONGLONG now = GetTickCount64();
  int duration = std::max(1, animation_.durationMs);
  double eased = std::clamp(static_cast<double>(now - animation_.startTick) / duration, 0.0, 1.0);

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
  if (eased >= 1.0) {
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
  if (state_.expanded) {
    state_.w = currentRect_.right - currentRect_.left;
    state_.h = currentRect_.bottom - currentRect_.top;
    SetTaskbarVisible(true);
    ShowHotZone(false);
  } else {
    SetTaskbarVisible(false);
    HideMainWindow();
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
  if (!hotZone_ || !hotZoneVisible_) return false;
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
