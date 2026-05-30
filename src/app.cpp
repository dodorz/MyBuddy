#include "app.h"

#include "config.h"
#include "state.h"
#include "version.h"

#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>

namespace {
constexpr UINT kTrayMsg = WM_APP + 1;
constexpr UINT kTrayIconId = 1001;
constexpr UINT kTrayOpenCmd = 2001;
constexpr UINT kTrayExitCmd = 2002;
constexpr int kListRowHeight = 28;
constexpr int kGroupIndent = 14;
constexpr int kFileIndent = 28;
constexpr int kGroupAddWidth = 36;
constexpr wchar_t kMainClass[] = L"MyBuddyMainClass";

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

std::wstring FormatFileTime(const FILETIME& ft) {
  SYSTEMTIME utc{};
  SYSTEMTIME local{};
  FileTimeToSystemTime(&ft, &utc);
  SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local);
  wchar_t buffer[64];
  wsprintfW(buffer, L"%04d-%02d-%02d %02d:%02d",
    local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute);
  return buffer;
}

std::wstring GetGroupStatusMessage(NoteGroupLoadState state, const NoteGroupConfig& group) {
  switch (state) {
    case NoteGroupLoadState::MissingDirectory:
      return L"Directory not found: " + group.path;
    case NoteGroupLoadState::Empty:
      return L"No matching notes in this group.";
    case NoteGroupLoadState::Ok:
    default:
      return L"";
  }
}
}

int App::Run(HINSTANCE instance, int showCmd) {
  instance_ = instance;

  WNDCLASSW wc{};
  wc.lpfnWndProc = App::MainWndProc;
  wc.hInstance = instance_;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszClassName = kMainClass;
  wc.cbWndExtra = sizeof(LONG_PTR);
  RegisterClassW(&wc);

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

LRESULT CALLBACK App::ListBoxProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  return self ? self->HandleListBoxMessage(hwnd, msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT App::HandleMainMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_CREATE: {
      LoadConfig();
      stateLoaded_ = LoadState();
      if (!stateLoaded_) {
        InitializeDefaultState();
      }
      CreateControls();
      ApplySavedGeometry();
      RefreshNotes();
      return 0;
    }
    case WM_SIZE:
      LayoutControls();
      return 0;
    case WM_MEASUREITEM:
      if (wp == 1) {
        auto* mi = reinterpret_cast<MEASUREITEMSTRUCT*>(lp);
        mi->itemHeight = kListRowHeight;
        return TRUE;
      }
      return FALSE;
    case WM_DRAWITEM:
      if (wp == 1) {
        DrawListItem(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
        return TRUE;
      }
      return FALSE;
    case WM_COMMAND:
      if (LOWORD(wp) == kTrayOpenCmd) {
        ShowMainWindow();
      } else if (LOWORD(wp) == kTrayExitCmd) {
        ExitFromTray();
      }
      return 0;
    case WM_CONTEXTMENU:
      if (reinterpret_cast<HWND>(wp) == listBox_) {
        POINT screenPt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (screenPt.x == -1 && screenPt.y == -1) {
          screenPt.x = 20;
          screenPt.y = 20;
          ClientToScreen(listBox_, &screenPt);
        }
        POINT clientPt = screenPt;
        ScreenToClient(listBox_, &clientPt);
        HandleListRightClick(clientPt);
        return 0;
      }
      break;
    case WM_MOVE:
    case WM_EXITSIZEMOVE: {
      RECT r{};
      GetWindowRect(hwnd_, &r);
      state_.x = r.left;
      state_.y = r.top;
      state_.w = r.right - r.left;
      state_.h = r.bottom - r.top;
      SaveState();
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
          ShowMainWindow();
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

LRESULT App::HandleListBoxMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_LBUTTONUP: {
      POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
      HandleListLeftClick(pt);
      return 0;
    }
    case WM_RBUTTONUP: {
      POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
      HandleListRightClick(pt);
      return 0;
    }
  }
  return CallWindowProcW(originalListBoxProc_, hwnd, msg, wp, lp);
}

bool App::LoadConfig() {
  std::wstring path = GetProgramConfigPath();
  if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
    path = GetFallbackConfigPath();
  }
  LoadAppConfig(config_, GetProgramConfigPath(), GetFallbackConfigPath());
  LoadNotesConfig(path, notesConfig_);
  return true;
}

bool App::LoadState() {
  return LoadAppState(state_, GetStatePath());
}

void App::SaveState() const {
  SaveAppState(state_, GetStatePath());
}

void App::InitializeDefaultState() {
  state_.version = 3;
  state_.x = 120;
  state_.y = 120;
  state_.w = 420;
  state_.h = 640;
  state_.taskbarVisible = true;
}

void App::SetTaskbarVisible(bool visible) {
  state_.taskbarVisible = visible;
  LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
  ex &= ~(WS_EX_APPWINDOW | WS_EX_TOOLWINDOW);
  ex |= visible ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW;
  SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex);
  SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void App::ShowToTray() {
  SetTaskbarVisible(false);
  HideMainWindow();
  SaveState();
}

void App::ExitFromTray() {
  RemoveTrayIcon(hwnd_);
  SaveState();
  DestroyWindow(hwnd_);
}

void App::ShowTrayMenu(POINT pt) {
  ShowTrayContextMenu(hwnd_, pt);
}

void App::ShowMainWindow() {
  SetTaskbarVisible(true);
  ShowWindow(hwnd_, SW_SHOW);
  SetForegroundWindow(hwnd_);
}

void App::HideMainWindow() {
  ShowWindow(hwnd_, SW_HIDE);
}

void App::ApplySavedGeometry() {
  SetWindowPos(hwnd_, HWND_TOPMOST, state_.x, state_.y, state_.w, state_.h, SWP_NOACTIVATE);
}

void App::CreateControls() {
  listBox_ = CreateWindowExW(
    WS_EX_CLIENTEDGE,
    L"LISTBOX",
    L"",
    WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
    0, 0, 0, 0,
    hwnd_,
    reinterpret_cast<HMENU>(1),
    instance_,
    nullptr);
  SetWindowLongPtrW(listBox_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  originalListBoxProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(listBox_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(App::ListBoxProc)));
}

void App::LayoutControls() {
  RECT rc{};
  GetClientRect(hwnd_, &rc);
  if (listBox_) {
    MoveWindow(listBox_, 0, 0, rc.right - rc.left, rc.bottom - rc.top, TRUE);
  }
}

void App::RefreshNotes() {
  notesByGroup_.clear();
  groupStates_.clear();
  expandedGroups_.clear();
  globalStatusMessage_.clear();
  if (notesConfig_.groups.empty()) {
    globalStatusMessage_ = L"No note groups configured. Add [note_group.<id>] sections to config.ini.";
  }
  for (const NoteGroupConfig& group : notesConfig_.groups) {
    std::vector<NoteFile> files;
    NoteGroupLoadState state = NoteGroupLoadState::Ok;
    LoadNoteFiles(group, files, &state);
    notesByGroup_.push_back(std::move(files));
    groupStates_.push_back(state);
    expandedGroups_.push_back(group.expanded);
  }
  RebuildVisibleRows();
}

void App::RefreshGroup(int groupIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(notesConfig_.groups.size())) return;
  NoteGroupLoadState state = NoteGroupLoadState::Ok;
  LoadNoteFiles(notesConfig_.groups[groupIndex], notesByGroup_[groupIndex], &state);
  groupStates_[groupIndex] = state;
  RebuildVisibleRows();
}

void App::RebuildVisibleRows() {
  visibleRows_.clear();
  if (!globalStatusMessage_.empty()) {
    visibleRows_.push_back({VisibleRow::Type::GlobalMessage, -1, -1});
  }
  for (int i = 0; i < static_cast<int>(notesConfig_.groups.size()); ++i) {
    visibleRows_.push_back({VisibleRow::Type::Group, i, -1});
    if (!expandedGroups_[i]) continue;
    if (groupStates_[i] != NoteGroupLoadState::Ok || notesByGroup_[i].empty()) {
      visibleRows_.push_back({VisibleRow::Type::GroupMessage, i, -1});
      continue;
    }
    for (int j = 0; j < static_cast<int>(notesByGroup_[i].size()); ++j) {
      visibleRows_.push_back({VisibleRow::Type::File, i, j});
    }
  }

  SendMessageW(listBox_, WM_SETREDRAW, FALSE, 0);
  SendMessageW(listBox_, LB_RESETCONTENT, 0, 0);
  for (size_t i = 0; i < visibleRows_.size(); ++i) {
    SendMessageW(listBox_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L""));
  }
  SendMessageW(listBox_, WM_SETREDRAW, TRUE, 0);
  InvalidateList();
}

void App::InvalidateList() {
  if (listBox_) InvalidateRect(listBox_, nullptr, TRUE);
}

RECT App::GetRowRect(int index) const {
  RECT rc{};
  SendMessageW(listBox_, LB_GETITEMRECT, index, reinterpret_cast<LPARAM>(&rc));
  return rc;
}

RECT App::GetGroupToggleRect(const RECT& rowRect) const {
  return RECT{ rowRect.left + 4, rowRect.top + 4, rowRect.left + 24, rowRect.bottom - 4 };
}

RECT App::GetGroupAddRect(const RECT& rowRect) const {
  return RECT{ rowRect.right - kGroupAddWidth - 8, rowRect.top + 4, rowRect.right - 8, rowRect.bottom - 4 };
}

void App::DrawListItem(const DRAWITEMSTRUCT* dis) {
  if (dis->itemID == static_cast<UINT>(-1) || dis->itemID >= visibleRows_.size()) return;

  const VisibleRow& row = visibleRows_[dis->itemID];
  HDC dc = dis->hDC;
  RECT rc = dis->rcItem;

  HBRUSH bg = CreateSolidBrush(row.type == VisibleRow::Type::Group ? RGB(240, 244, 248) : RGB(255, 255, 255));
  FillRect(dc, &rc, bg);
  DeleteObject(bg);

  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(32, 32, 32));

  if (row.type == VisibleRow::Type::GlobalMessage) {
    RECT textRc = rc;
    textRc.left += 12;
    SetTextColor(dc, RGB(112, 112, 112));
    DrawTextW(dc, globalStatusMessage_.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  } else if (row.type == VisibleRow::Type::Group) {
    const NoteGroupConfig& group = notesConfig_.groups[row.groupIndex];
    RECT toggleRc = GetGroupToggleRect(rc);
    RECT addRc = GetGroupAddRect(rc);
    RECT textRc = rc;
    textRc.left += kGroupIndent + 14;
    textRc.right = addRc.left - 8;

    DrawTextW(dc, expandedGroups_[row.groupIndex] ? L"v" : L">", -1, &toggleRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    DrawTextW(dc, group.title.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    Rectangle(dc, addRc.left, addRc.top, addRc.right, addRc.bottom);
    DrawTextW(dc, L"+", -1, &addRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
  } else if (row.type == VisibleRow::Type::GroupMessage) {
    RECT textRc = rc;
    textRc.left += kFileIndent;
    textRc.right -= 8;
    SetTextColor(dc, RGB(112, 112, 112));
    std::wstring message = GetGroupStatusMessage(groupStates_[row.groupIndex], notesConfig_.groups[row.groupIndex]);
    DrawTextW(dc, message.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
  } else {
    const NoteFile& file = notesByGroup_[row.groupIndex][row.fileIndex];
    RECT nameRc = rc;
    nameRc.left += kFileIndent;
    nameRc.right -= 110;
    RECT timeRc = rc;
    timeRc.left = nameRc.right + 8;
    timeRc.right -= 8;

    DrawTextW(dc, file.name.c_str(), -1, &nameRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
    std::wstring timeText = FormatFileTime(file.modifiedTime);
    DrawTextW(dc, timeText.c_str(), -1, &timeRc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
  }
}

int App::HitTestRow(POINT pt) const {
  LRESULT hit = SendMessageW(listBox_, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
  if (HIWORD(hit) != 0) return -1;
  int index = LOWORD(hit);
  if (index < 0 || index >= static_cast<int>(visibleRows_.size())) return -1;
  return index;
}

void App::HandleListLeftClick(POINT pt) {
  int index = HitTestRow(pt);
  if (index < 0) return;

  const VisibleRow& row = visibleRows_[index];
  RECT rowRect = GetRowRect(index);
  if (row.type == VisibleRow::Type::Group) {
    RECT addRc = GetGroupAddRect(rowRect);
    RECT toggleRc = GetGroupToggleRect(rowRect);
    if (PtInRect(&addRc, pt)) {
      CreateNoteForGroup(row.groupIndex);
    } else if (PtInRect(&toggleRc, pt) || PtInRect(&rowRect, pt)) {
      ToggleGroup(row.groupIndex);
    }
    return;
  }

  if (row.type != VisibleRow::Type::File) return;
  OpenFileNote(row.groupIndex, row.fileIndex);
}

void App::HandleListRightClick(POINT pt) {
  int index = HitTestRow(pt);
  if (index < 0) return;
  const VisibleRow& row = visibleRows_[index];
  if (row.type != VisibleRow::Type::Group && row.type != VisibleRow::Type::File) return;

  POINT screenPt = pt;
  ClientToScreen(listBox_, &screenPt);
  if (row.type == VisibleRow::Type::Group) {
    RunGroupMenu(row.groupIndex, screenPt);
  } else {
    RunFileMenu(row.groupIndex, row.fileIndex, screenPt);
  }
}

void App::ToggleGroup(int groupIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(expandedGroups_.size())) return;
  expandedGroups_[groupIndex] = !expandedGroups_[groupIndex];
  RebuildVisibleRows();
}

void App::CreateNoteForGroup(int groupIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(notesConfig_.groups.size())) return;
  std::wstring createdPath;
  std::wstring errorMessage;
  const NoteGroupConfig& group = notesConfig_.groups[groupIndex];
  if (!CreateNoteInGroup(group, createdPath, &errorMessage)) {
    MessageBoxW(hwnd_, errorMessage.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }
  RefreshGroup(groupIndex);
  if (!group.defaultFileAction.empty()) {
    NoteFile created{};
    created.path = createdPath;
    created.dir = group.path;
    auto pos = createdPath.find_last_of(L"\\/");
    created.name = pos == std::wstring::npos ? createdPath : createdPath.substr(pos + 1);
    created.stem = created.name;
    auto dot = created.stem.find_last_of(L'.');
    if (dot != std::wstring::npos) created.stem = created.stem.substr(0, dot);
    auto actionIt = notesConfig_.actions.find(group.defaultFileAction);
    if (actionIt != notesConfig_.actions.end()) {
      std::wstring command;
      if (!ExecuteAction(actionIt->second, group, &created, &errorMessage, &command)) {
        std::wstring message = L"Failed to run action:\n" + actionIt->second.title + L"\n\nCommand:\n" + command + L"\n\n" + errorMessage;
        MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
      }
    }
  }
}

void App::OpenFileNote(int groupIndex, int fileIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(notesConfig_.groups.size())) return;
  const NoteGroupConfig& group = notesConfig_.groups[groupIndex];
  if (group.defaultFileAction.empty()) return;
  auto actionIt = notesConfig_.actions.find(group.defaultFileAction);
  if (actionIt == notesConfig_.actions.end()) return;
  std::wstring errorMessage;
  std::wstring command;
  if (!ExecuteAction(actionIt->second, group, &notesByGroup_[groupIndex][fileIndex], &errorMessage, &command)) {
    std::wstring message = L"Failed to run action:\n" + actionIt->second.title + L"\n\nCommand:\n" + command + L"\n\n" + errorMessage;
    MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
  }
}

void App::RunGroupMenu(int groupIndex, POINT screenPt) {
  const NoteGroupConfig& group = notesConfig_.groups[groupIndex];
  HMENU menu = CreatePopupMenu();
  if (!menu) return;
  constexpr UINT kMenuNew = 4001;
  constexpr UINT kMenuRefresh = 4002;
  UINT nextActionId = 4100;

  AppendMenuW(menu, MF_STRING, kMenuNew, L"New Note");
  AppendMenuW(menu, MF_STRING, kMenuRefresh, L"Refresh");
  if (!group.groupActions.empty()) {
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  }

  std::unordered_map<UINT, std::wstring> actionIds;
  for (const std::wstring& actionId : group.groupActions) {
    auto it = notesConfig_.actions.find(actionId);
    if (it == notesConfig_.actions.end()) continue;
    AppendMenuW(menu, MF_STRING, nextActionId, it->second.title.c_str());
    actionIds[nextActionId] = actionId;
    ++nextActionId;
  }

  SetForegroundWindow(hwnd_);
  UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
    screenPt.x, screenPt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);

  if (cmd == kMenuNew) {
    CreateNoteForGroup(groupIndex);
  } else if (cmd == kMenuRefresh) {
    RefreshGroup(groupIndex);
  } else if (auto it = actionIds.find(cmd); it != actionIds.end()) {
    std::wstring errorMessage;
    std::wstring command;
    if (!ExecuteAction(notesConfig_.actions[it->second], group, nullptr, &errorMessage, &command)) {
      std::wstring message = L"Failed to run action:\n" + notesConfig_.actions[it->second].title + L"\n\nCommand:\n" + command + L"\n\n" + errorMessage;
      MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    }
  }
}

void App::RunFileMenu(int groupIndex, int fileIndex, POINT screenPt) {
  const NoteGroupConfig& group = notesConfig_.groups[groupIndex];
  const NoteFile& file = notesByGroup_[groupIndex][fileIndex];
  HMENU menu = CreatePopupMenu();
  if (!menu) return;

  UINT nextActionId = 5000;
  std::unordered_map<UINT, std::wstring> actionIds;

  if (!group.defaultFileAction.empty()) {
    auto it = notesConfig_.actions.find(group.defaultFileAction);
    if (it != notesConfig_.actions.end()) {
      AppendMenuW(menu, MF_STRING, nextActionId, it->second.title.c_str());
      actionIds[nextActionId] = group.defaultFileAction;
      ++nextActionId;
    }
  }

  for (const std::wstring& actionId : group.fileActions) {
    if (actionId == group.defaultFileAction) continue;
    auto it = notesConfig_.actions.find(actionId);
    if (it == notesConfig_.actions.end()) continue;
    AppendMenuW(menu, MF_STRING, nextActionId, it->second.title.c_str());
    actionIds[nextActionId] = actionId;
    ++nextActionId;
  }

  SetForegroundWindow(hwnd_);
  UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
    screenPt.x, screenPt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);
  if (auto it = actionIds.find(cmd); it != actionIds.end()) {
    std::wstring errorMessage;
    std::wstring command;
    if (!ExecuteAction(notesConfig_.actions[it->second], group, &file, &errorMessage, &command)) {
      std::wstring message = L"Failed to run action:\n" + notesConfig_.actions[it->second].title + L"\n\nCommand:\n" + command + L"\n\n" + errorMessage;
      MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    }
  }
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
