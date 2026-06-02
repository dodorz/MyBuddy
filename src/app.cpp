#include "app.h"

#include "resource.h"
#include "state.h"

#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>

#include <algorithm>

namespace {
constexpr UINT kTrayMsg = WM_APP + 1;
constexpr UINT kSingleInstanceShowMsg = WM_APP + 2;
constexpr int kGlobalHotKeyId = 0xB001;
constexpr UINT kAutoHideTimerId = 5001;
constexpr UINT kAnimationTimerId = 5002;
constexpr UINT kTrayIconId = 1001;
constexpr UINT kTrayOpenCmd = 2001;
constexpr UINT kTrayExitCmd = 2002;
constexpr int kListRowHeight = 30;
constexpr int kGroupIndent = 14;
constexpr int kFileIndent = 28;
constexpr int kGroupButtonWidth = 32;
constexpr int kGroupAddWidth = 32;
constexpr int kGroupClipboardWidth = 32;
constexpr UINT kToolbarButtonBaseId = 7000;
constexpr int kToolbarHeightPx = 32;
constexpr int kToolbarButtonSizePx = 24;
constexpr int kToolbarPaddingPx = 4;
constexpr wchar_t kMainClass[] = L"MyBuddyMainClass";
constexpr wchar_t kHotZoneClass[] = L"MyBuddyHotZoneClass";
constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\MyBuddy.SingleInstance";
constexpr int kSnapThresholdPx = 28;
constexpr int kHotZoneThicknessPx = 2;
constexpr int kAutoHideDelayMs = 420;
constexpr int kAutoHidePollMs = 80;
constexpr int kExpandDurationMs = 210;
constexpr int kCollapseDurationMs = 300;
constexpr int kAnimationTickMs = 16;

std::wstring EnsureTrailingSlash(std::wstring path) {
  if (!path.empty() && path.back() != L'\\') path.push_back(L'\\');
  return path;
}

std::wstring ReadIniString(const std::wstring& path, const wchar_t* section, const wchar_t* key, const wchar_t* def = L"") {
  wchar_t buffer[512];
  GetPrivateProfileStringW(section, key, def, buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
  return buffer;
}

std::wstring Trim(std::wstring value) {
  size_t start = 0;
  while (start < value.size() && iswspace(value[start])) ++start;
  size_t end = value.size();
  while (end > start && iswspace(value[end - 1])) --end;
  return value.substr(start, end - start);
}

std::wstring ToUpper(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(), towupper);
  return value;
}

std::wstring ToLower(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(), towlower);
  return value;
}

std::vector<std::wstring> SplitSemicolonList(const std::wstring& value) {
  std::vector<std::wstring> parts;
  std::wstring current;
  for (wchar_t ch : value) {
    if (ch == L';') {
      std::wstring token = Trim(current);
      if (!token.empty()) parts.push_back(token);
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  std::wstring token = Trim(current);
  if (!token.empty()) parts.push_back(token);
  return parts;
}

std::vector<std::wstring> ReadIniSectionNames(const std::wstring& path) {
  std::vector<std::wstring> sections;
  std::vector<wchar_t> buffer(65536);
  DWORD len = GetPrivateProfileSectionNamesW(buffer.data(), static_cast<DWORD>(buffer.size()), path.c_str());
  if (len == 0) return sections;

  const wchar_t* ptr = buffer.data();
  while (*ptr) {
    sections.emplace_back(ptr);
    ptr += wcslen(ptr) + 1;
  }
  return sections;
}

std::vector<std::wstring> SplitHotKeySpec(const std::wstring& spec) {
  std::vector<std::wstring> parts;
  std::wstring current;
  for (wchar_t ch : spec) {
    if (ch == L'+') {
      std::wstring token = Trim(current);
      if (!token.empty()) parts.push_back(token);
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  std::wstring token = Trim(current);
  if (!token.empty()) parts.push_back(token);
  return parts;
}

bool TryParseFunctionKey(const std::wstring& token, UINT& vk) {
  if (token.size() < 2 || token[0] != L'F') return false;
  int value = 0;
  for (size_t i = 1; i < token.size(); ++i) {
    if (!iswdigit(token[i])) return false;
    value = value * 10 + (token[i] - L'0');
  }
  if (value < 1 || value > 24) return false;
  vk = VK_F1 + (value - 1);
  return true;
}

bool ParseHotKeySpec(const std::wstring& spec, UINT& modifiers, UINT& vk) {
  modifiers = 0;
  vk = 0;
  std::vector<std::wstring> parts = SplitHotKeySpec(spec);
  if (parts.empty()) return false;

  for (const std::wstring& rawPart : parts) {
    const std::wstring part = ToUpper(rawPart);
    if (part == L"CTRL" || part == L"CONTROL") {
      modifiers |= MOD_CONTROL;
      continue;
    }
    if (part == L"ALT") {
      modifiers |= MOD_ALT;
      continue;
    }
    if (part == L"SHIFT") {
      modifiers |= MOD_SHIFT;
      continue;
    }
    if (part == L"WIN" || part == L"WINDOWS" || part == L"META") {
      modifiers |= MOD_WIN;
      continue;
    }

    UINT parsedVk = 0;
    if (part.size() == 1) {
      wchar_t ch = part[0];
      if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) {
        parsedVk = static_cast<UINT>(ch);
      }
    } else if (TryParseFunctionKey(part, parsedVk)) {
    } else if (part == L"SPACE") {
      parsedVk = VK_SPACE;
    }

    if (parsedVk == 0 || vk != 0) return false;
    vk = parsedVk;
  }

  return modifiers != 0 && vk != 0;
}

std::wstring GetFileNameFromPath(const std::wstring& path) {
  const size_t pos = path.find_last_of(L"\\/");
  return pos == std::wstring::npos ? path : path.substr(pos + 1);
}

std::wstring GetFileStemFromName(const std::wstring& name) {
  const size_t pos = name.find_last_of(L'.');
  return pos == std::wstring::npos ? name : name.substr(0, pos);
}

App::ToolbarScope ParseToolbarScope(const std::wstring& value) {
  const std::wstring normalized = ToLower(Trim(value));
  if (normalized == L"group") return App::ToolbarScope::Group;
  if (normalized == L"file" || normalized == L"selection") return App::ToolbarScope::File;
  return App::ToolbarScope::Global;
}

bool IsSupportedToolbarIcon(const std::wstring& value) {
  const std::wstring normalized = ToLower(Trim(value));
  return normalized == L"touch" || normalized == L"proxy";
}

void DrawTouchToolbarIcon(HDC dc, const RECT& rc, COLORREF color) {
  HPEN pen = CreatePen(PS_SOLID, 2, color);
  HGDIOBJ oldPen = SelectObject(dc, pen);
  HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));

  Rectangle(dc, rc.left + 3, rc.top + 2, rc.left + 11, rc.top + 13);
  MoveToEx(dc, rc.left + 8, rc.top + 2, nullptr);
  LineTo(dc, rc.left + 11, rc.top + 5);
  MoveToEx(dc, rc.left + 8, rc.top + 2, nullptr);
  LineTo(dc, rc.left + 8, rc.top + 5);
  MoveToEx(dc, rc.left + 8, rc.top + 5, nullptr);
  LineTo(dc, rc.left + 11, rc.top + 5);

  MoveToEx(dc, rc.left + 12, rc.top + 12, nullptr);
  LineTo(dc, rc.left + 12, rc.top + 6);
  MoveToEx(dc, rc.left + 9, rc.top + 9, nullptr);
  LineTo(dc, rc.left + 12, rc.top + 6);
  LineTo(dc, rc.left + 15, rc.top + 9);

  SelectObject(dc, oldBrush);
  SelectObject(dc, oldPen);
  DeleteObject(pen);
}

void DrawProxyToolbarIcon(HDC dc, const RECT& rc, COLORREF color) {
  HPEN pen = CreatePen(PS_SOLID, 2, color);
  HBRUSH brush = CreateSolidBrush(color);
  HGDIOBJ oldPen = SelectObject(dc, pen);
  HGDIOBJ oldBrush = SelectObject(dc, brush);

  Ellipse(dc, rc.left + 2, rc.top + 6, rc.left + 6, rc.top + 10);
  Ellipse(dc, rc.left + 12, rc.top + 6, rc.left + 16, rc.top + 10);
  MoveToEx(dc, rc.left + 6, rc.top + 8, nullptr);
  LineTo(dc, rc.left + 12, rc.top + 8);

  SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
  Arc(dc, rc.left + 4, rc.top + 2, rc.left + 14, rc.top + 12, rc.left + 8, rc.top + 2, rc.left + 12, rc.top + 6);
  Arc(dc, rc.left + 4, rc.top + 6, rc.left + 14, rc.top + 16, rc.left + 12, rc.top + 16, rc.left + 8, rc.top + 12);
  MoveToEx(dc, rc.left + 12, rc.top + 6, nullptr);
  LineTo(dc, rc.left + 10, rc.top + 4);
  MoveToEx(dc, rc.left + 12, rc.top + 6, nullptr);
  LineTo(dc, rc.left + 10, rc.top + 6);
  MoveToEx(dc, rc.left + 8, rc.top + 12, nullptr);
  LineTo(dc, rc.left + 10, rc.top + 12);
  MoveToEx(dc, rc.left + 8, rc.top + 12, nullptr);
  LineTo(dc, rc.left + 10, rc.top + 14);

  SelectObject(dc, oldBrush);
  SelectObject(dc, oldPen);
  DeleteObject(brush);
  DeleteObject(pen);
}

struct MarkdownCheckbox {
  enum class State {
    None,
    Unchecked,
    Checked,
  };

  State state = State::None;
  std::wstring indent;
  std::wstring label;
};

std::wstring ExpandTabsForDisplay(const std::wstring& text, int tabWidth = 4) {
  std::wstring expanded;
  int column = 0;
  for (wchar_t ch : text) {
    if (ch == L'\t') {
      const int spaces = tabWidth - (column % tabWidth);
      expanded.append(spaces, L' ');
      column += spaces;
    } else {
      expanded.push_back(ch);
      ++column;
    }
  }
  return expanded;
}

MarkdownCheckbox ParseMarkdownCheckbox(const std::wstring& text) {
  MarkdownCheckbox checkbox{};
  size_t start = 0;
  while (start < text.size() && (text[start] == L' ' || text[start] == L'\t')) ++start;
  checkbox.indent = text.substr(0, start);

  const std::wstring body = text.substr(start);
  auto parse = [&](wchar_t marker, MarkdownCheckbox::State state) -> bool {
    if (body.size() < 5) return false;
    if (body[0] != L'-' || body[1] != L' ' || body[2] != L'[' || body[3] != marker || body[4] != L']') {
      return false;
    }
    checkbox.state = state;
    if (body.size() == 5) {
      checkbox.label.clear();
    } else if (body.size() >= 6 && body[5] == L' ') {
      checkbox.label = body.substr(6);
    } else {
      checkbox.state = MarkdownCheckbox::State::None;
      return false;
    }
    return true;
  };

  if (parse(L' ', MarkdownCheckbox::State::Unchecked)) return checkbox;
  if (parse(L'x', MarkdownCheckbox::State::Checked)) return checkbox;
  if (parse(L'X', MarkdownCheckbox::State::Checked)) return checkbox;
  return checkbox;
}

bool BuildGroupSourceFile(const NoteGroupConfig& group, NoteFile& file) {
  if (group.type != NoteGroupType::TextLines) return false;
  WIN32_FILE_ATTRIBUTE_DATA data{};
  if (!GetFileAttributesExW(group.path.c_str(), GetFileExInfoStandard, &data)) return false;
  file = NoteFile{};
  file.path = group.path;
  const size_t pos = group.path.find_last_of(L"\\/");
  file.dir = pos == std::wstring::npos ? L"" : group.path.substr(0, pos);
  file.name = GetFileNameFromPath(group.path);
  file.stem = GetFileStemFromName(file.name);
  file.displayName = file.name;
  file.itemText.clear();
  file.lineNumber = 0;
  file.createdTime = data.ftCreationTime;
  file.modifiedTime = data.ftLastWriteTime;
  return true;
}

bool MovePathToRecycleBin(const std::wstring& path, std::wstring* errorMessage) {
  if (path.empty()) {
    if (errorMessage) *errorMessage = L"Path is empty.";
    return false;
  }

  std::vector<wchar_t> from(path.begin(), path.end());
  from.push_back(L'\0');
  from.push_back(L'\0');

  SHFILEOPSTRUCTW op{};
  op.wFunc = FO_DELETE;
  op.pFrom = from.data();
  op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

  int result = SHFileOperationW(&op);
  if (result != 0) {
    if (errorMessage) *errorMessage = L"Failed to move item to Recycle Bin. Error code: " + std::to_wstring(result);
    return false;
  }
  if (op.fAnyOperationsAborted) {
    if (errorMessage) *errorMessage = L"Delete operation was aborted.";
    return false;
  }
  return true;
}

void AddTrayIcon(HWND hwnd) {
  NOTIFYICONDATAW nid{};
  nid.cbSize = sizeof(nid);
  nid.hWnd = hwnd;
  nid.uID = kTrayIconId;
  nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  nid.uCallbackMessage = kTrayMsg;
  nid.hIcon = reinterpret_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_MYBUDDY_APP),
    IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
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

int MeasureTextWidth(HDC dc, const std::wstring& text) {
  if (text.empty()) return 0;
  SIZE size{};
  GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
  return size.cx;
}

std::wstring FormatAdaptiveFileTime(HDC dc, const FILETIME& ft, int availableWidth) {
  SYSTEMTIME utc{};
  SYSTEMTIME local{};
  FileTimeToSystemTime(&ft, &utc);
  SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local);

  wchar_t full[32];
  wchar_t shortYear[32];
  wchar_t shortYearHour[32];
  wchar_t dateOnly[24];
  wchar_t compactDate[24];
  wchar_t monthDay[16];
  wsprintfW(full, L"%04d-%02d-%02d %02d:%02d",
    local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute);
  wsprintfW(shortYear, L"%02d-%02d-%02d %02d:%02d",
    local.wYear % 100, local.wMonth, local.wDay, local.wHour, local.wMinute);
  wsprintfW(shortYearHour, L"%02d-%02d-%02d %02d",
    local.wYear % 100, local.wMonth, local.wDay, local.wHour);
  wsprintfW(dateOnly, L"%02d-%02d-%02d",
    local.wYear % 100, local.wMonth, local.wDay);
  wsprintfW(compactDate, L"%02d%02d%02d",
    local.wYear % 100, local.wMonth, local.wDay);
  wsprintfW(monthDay, L"%02d%02d", local.wMonth, local.wDay);

  const std::wstring variants[] = {
    full,
    shortYear,
    shortYearHour,
    dateOnly,
    compactDate,
    monthDay,
    L""
  };

  for (const std::wstring& variant : variants) {
    if (MeasureTextWidth(dc, variant) <= availableWidth) return variant;
  }
  return L"";
}

std::wstring GetGroupStatusMessage(NoteGroupLoadState state, const NoteGroupConfig& group) {
  switch (state) {
    case NoteGroupLoadState::MissingPath:
      return group.type == NoteGroupType::Directory
        ? L"Directory not found: " + group.path
        : L"File not found: " + group.path;
    case NoteGroupLoadState::Empty:
      return group.type == NoteGroupType::Directory
        ? L"No matching notes in this group."
        : L"No non-empty lines in this group file.";
    case NoteGroupLoadState::Ok:
    default:
      return L"";
  }
}

std::string EncodeUtf8(const std::wstring& text) {
  if (text.empty()) return {};
  int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (size <= 0) return {};
  std::string utf8(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), size, nullptr, nullptr);
  return utf8;
}

bool WriteUtf8TextFile(const std::wstring& path, const std::wstring& text, std::wstring* errorMessage) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    if (errorMessage) *errorMessage = L"Failed to write temporary note: " + std::to_wstring(GetLastError());
    return false;
  }

  const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
  DWORD written = 0;
  BOOL ok = WriteFile(file, bom, static_cast<DWORD>(std::size(bom)), &written, nullptr);
  if (!ok) {
    if (errorMessage) *errorMessage = L"Failed to write temporary note: " + std::to_wstring(GetLastError());
    CloseHandle(file);
    return false;
  }

  std::string utf8 = EncodeUtf8(text);
  if (!utf8.empty()) {
    ok = WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    if (!ok) {
      if (errorMessage) *errorMessage = L"Failed to write temporary note: " + std::to_wstring(GetLastError());
      CloseHandle(file);
      return false;
    }
  }

  CloseHandle(file);
  return true;
}

bool ReadClipboardText(HWND hwnd, std::wstring& text, std::wstring* errorMessage) {
  text.clear();
  if (!OpenClipboard(hwnd)) {
    if (errorMessage) *errorMessage = L"Failed to open clipboard.";
    return false;
  }

  HANDLE data = GetClipboardData(CF_UNICODETEXT);
  if (!data) {
    if (errorMessage) *errorMessage = L"Clipboard does not contain text.";
    CloseClipboard();
    return false;
  }

  const wchar_t* buffer = static_cast<const wchar_t*>(GlobalLock(data));
  if (!buffer) {
    if (errorMessage) *errorMessage = L"Failed to read clipboard text.";
    CloseClipboard();
    return false;
  }

  text = buffer;
  GlobalUnlock(data);
  CloseClipboard();

  if (text.empty()) {
    if (errorMessage) *errorMessage = L"Clipboard text is empty.";
    return false;
  }
  return true;
}

bool GetFileSnapshot(const std::wstring& path, FILETIME& lastWriteTime, ULONGLONG& size) {
  WIN32_FILE_ATTRIBUTE_DATA data{};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return false;
  lastWriteTime = data.ftLastWriteTime;
  size = (static_cast<ULONGLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
  return true;
}

auto FindNewNoteEditAction(const NotesConfig& config) {
  auto it = config.actions.find(L"Edit");
  if (it != config.actions.end() && IsActionTargetCompatible(it->second, reinterpret_cast<const NoteFile*>(1))) {
    return it;
  }
  it = config.actions.find(L"edit");
  if (it != config.actions.end() && IsActionTargetCompatible(it->second, reinterpret_cast<const NoteFile*>(1))) {
    return it;
  }
  return config.actions.end();
}
}

int App::Run(HINSTANCE instance, int showCmd) {
  instance_ = instance;
  INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_WIN95_CLASSES };
  InitCommonControlsEx(&icc);
  singleInstanceMutex_ = CreateMutexW(nullptr, TRUE, kSingleInstanceMutexName);
  if (!singleInstanceMutex_) return 0;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND existing = FindWindowW(kMainClass, nullptr);
    if (existing) {
      DWORD processId = 0;
      GetWindowThreadProcessId(existing, &processId);
      if (processId != 0) AllowSetForegroundWindow(processId);
      PostMessageW(existing, kSingleInstanceShowMsg, 0, 0);
    }
    CloseHandle(singleInstanceMutex_);
    singleInstanceMutex_ = nullptr;
    return 0;
  }

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = App::MainWndProc;
  wc.hInstance = instance_;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.hIcon = reinterpret_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_MYBUDDY_APP),
    IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
  wc.hIconSm = reinterpret_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_MYBUDDY_APP),
    IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
  wc.lpszClassName = kMainClass;
  wc.cbWndExtra = sizeof(LONG_PTR);
  RegisterClassExW(&wc);

  WNDCLASSEXW hotZoneClass{};
  hotZoneClass.cbSize = sizeof(hotZoneClass);
  hotZoneClass.lpfnWndProc = App::HotZoneWndProc;
  hotZoneClass.hInstance = instance_;
  hotZoneClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  hotZoneClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  hotZoneClass.lpszClassName = kHotZoneClass;
  hotZoneClass.cbWndExtra = sizeof(LONG_PTR);
  RegisterClassExW(&hotZoneClass);

  const std::wstring title = L"MyBuddy";

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

  SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(wc.hIcon));
  SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(wc.hIconSm));

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
      RegisterHotKey(hwnd_, kGlobalHotKeyId, hotKeyModifiers_, hotKeyVk_);
      CreateHotZoneWindow();
      CreateFonts();
      CreateControls();
      ApplySavedGeometry();
      RefreshNotes();
      lastPointerInsideTick_ = GetTickCount64();
      SetTimer(hwnd_, kAutoHideTimerId, kAutoHidePollMs, nullptr);
      return 0;
    }
    case WM_SIZE:
      LayoutControls();
      return 0;
    case WM_ENTERSIZEMOVE:
      inMoveSize_ = true;
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
      if (wp >= kToolbarButtonBaseId && wp < kToolbarButtonBaseId + toolbarButtons_.size()) {
        DrawToolbarButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
        return TRUE;
      }
      return FALSE;
    case WM_COMMAND:
      if (LOWORD(wp) == 1 && HIWORD(wp) == LBN_SELCHANGE) {
        currentRowIndex_ = GetCurrentRowIndex();
        UpdateToolbarButtons();
      } else if (LOWORD(wp) >= kToolbarButtonBaseId && LOWORD(wp) < kToolbarButtonBaseId + toolbarButtons_.size() &&
                 HIWORD(wp) == BN_CLICKED) {
        RunToolbarButton(LOWORD(wp) - kToolbarButtonBaseId);
      } else if (LOWORD(wp) == kTrayOpenCmd) {
        if (IsDocked()) {
          RequestExpand(true);
        } else {
          ShowMainWindow(true);
        }
      } else if (LOWORD(wp) == kTrayExitCmd) {
        ExitFromTray();
      }
      return 0;
    case kSingleInstanceShowMsg:
      if (IsDocked()) {
        RequestExpand(true);
      } else {
        ShowMainWindow(true);
      }
      return 0;
    case WM_HOTKEY:
      if (wp == kGlobalHotKeyId) {
        if (IsDocked()) {
          RequestExpand(true);
        } else {
          ShowMainWindow(true);
        }
        return 0;
      }
      break;
    case WM_TIMER:
      if (wp == kAnimationTimerId) {
        TickAnimation();
        return 0;
      }
      if (wp == kAutoHideTimerId) {
        PollAutoHide();
        return 0;
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
    case WM_EXITSIZEMOVE: {
      inMoveSize_ = false;
      if (animation_.active || suppressWindowTracking_) return 0;
      RECT r{};
      GetWindowRect(hwnd_, &r);
      CommitVisibleRect(r, true);
      SyncHotZone();
      return 0;
    }
    case WM_MOVE: {
      if (animation_.active || suppressWindowTracking_ || inMoveSize_ || !state_.expanded || trayHidden_) return 0;
      RECT r{};
      GetWindowRect(hwnd_, &r);
      CommitVisibleRect(r, false);
      SyncHotZone();
      return 0;
    }
    case WM_CLOSE:
      ShowToTray();
      return 0;
    case WM_DESTROY:
      KillTimer(hwnd_, kAnimationTimerId);
      KillTimer(hwnd_, kAutoHideTimerId);
      UnregisterHotKey(hwnd_, kGlobalHotKeyId);
      DestroyHotZoneWindow();
      if (singleInstanceMutex_) {
        CloseHandle(singleInstanceMutex_);
        singleInstanceMutex_ = nullptr;
      }
      DestroyToolbarButtons();
      DestroyFonts();
      PostQuitMessage(0);
      return 0;
    default:
      if (msg == kTrayMsg) {
        if (LOWORD(lp) == WM_LBUTTONUP || LOWORD(lp) == WM_LBUTTONDBLCLK) {
          if (IsDocked()) {
            RequestExpand(true);
          } else {
            ShowMainWindow(true);
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
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
      RequestExpand(msg != WM_MOUSEMOVE);
      return 0;
    case WM_NCHITTEST:
      return HTCLIENT;
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
  const std::wstring programPath = GetProgramConfigPath();
  const std::wstring fallbackPath = GetFallbackConfigPath();
  std::wstring path = programPath;
  if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
    path = fallbackPath;
  }
  currentConfigPath_ = path;

  hotKeySpec_ = ReadIniString(path, L"app", L"globalHotKey", L"Ctrl+Alt+B");
  if (!ParseHotKeySpec(hotKeySpec_, hotKeyModifiers_, hotKeyVk_)) {
    hotKeySpec_ = L"Ctrl+Alt+B";
    hotKeyModifiers_ = MOD_CONTROL | MOD_ALT;
    hotKeyVk_ = 'B';
  }

  std::vector<ToolbarButtonConfig> loadedToolbarButtons;
  std::vector<std::wstring> toolbarButtonIds = SplitSemicolonList(ReadIniString(path, L"toolbar", L"buttons"));
  if (toolbarButtonIds.empty()) {
    for (const std::wstring& section : ReadIniSectionNames(path)) {
      if (section.rfind(L"toolbar_button.", 0) == 0 && section.size() > 15) {
        toolbarButtonIds.push_back(section.substr(15));
      }
    }
  }
  for (const std::wstring& buttonId : toolbarButtonIds) {
    const std::wstring section = L"toolbar_button." + buttonId;
    ToolbarButtonConfig button{};
    button.id = buttonId;
    button.title = ReadIniString(path, section.c_str(), L"title", buttonId.c_str());
    button.icon = ToLower(ReadIniString(path, section.c_str(), L"icon"));
    button.command = ReadIniString(path, section.c_str(), L"command");
    button.scope = ParseToolbarScope(ReadIniString(path, section.c_str(), L"scope", L"global"));
    if (button.command.empty() || !IsSupportedToolbarIcon(button.icon)) continue;
    loadedToolbarButtons.push_back(std::move(button));
  }
  toolbarButtons_ = std::move(loadedToolbarButtons);

  NotesConfig loadedNotesConfig{};
  if (LoadNotesConfig(path, loadedNotesConfig)) {
    notesConfig_ = std::move(loadedNotesConfig);
  } else {
    notesConfig_ = NotesConfig{};
  }
  if (hwnd_ && listBox_) {
    CreateToolbarButtons();
    LayoutControls();
    UpdateToolbarButtons();
  }
  return true;
}

bool App::LoadState() {
  return LoadAppState(state_, GetStatePath());
}

void App::SaveState() const {
  SaveAppState(state_, GetStatePath());
}

void App::InitializeDefaultState() {
  state_.version = 4;
  state_.dockEdge = static_cast<int>(GetFallbackDockEdge());
  RECT work = GetWorkArea();
  state_.w = 420;
  state_.h = 640;
  state_.x = work.right - state_.w;
  state_.y = work.top + 80;
  state_.expanded = true;
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
  trayHidden_ = true;
  state_.expanded = true;
  animation_.active = false;
  KillTimer(hwnd_, kAnimationTimerId);
  SetTaskbarVisible(false);
  SyncHotZone();
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

void App::ShowMainWindow(bool activate) {
  trayHidden_ = false;
  SetTaskbarVisible(true);
  ShowWindow(hwnd_, IsIconic(hwnd_) ? SW_RESTORE : (activate ? SW_SHOW : SW_SHOWNOACTIVATE));
  if (activate) {
    SetForegroundWindow(hwnd_);
  }
}

void App::HideMainWindow() {
  ShowWindow(hwnd_, SW_HIDE);
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
  if (hotZone_) {
    SetLayeredWindowAttributes(hotZone_, 0, 1, LWA_ALPHA);
  }
}

void App::DestroyHotZoneWindow() {
  if (hotZone_) {
    DestroyWindow(hotZone_);
    hotZone_ = nullptr;
  }
}

RECT App::GetWorkArea() const {
  const HMONITOR monitor = MonitorFromWindow(hwnd_ ? hwnd_ : GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
  MONITORINFO info{ sizeof(info) };
  GetMonitorInfoW(monitor, &info);
  return info.rcWork;
}

App::DockEdge App::GetTaskbarDockEdge() const {
  APPBARDATA abd{};
  abd.cbSize = sizeof(abd);
  abd.hWnd = FindWindowW(L"Shell_TrayWnd", nullptr);
  if (!abd.hWnd || SHAppBarMessage(ABM_GETTASKBARPOS, &abd) == 0) {
    return DockEdge::Bottom;
  }

  switch (abd.uEdge) {
    case ABE_LEFT: return DockEdge::Left;
    case ABE_RIGHT: return DockEdge::Right;
    case ABE_TOP: return DockEdge::Top;
    case ABE_BOTTOM:
    default:
      return DockEdge::Bottom;
  }
}

App::DockEdge App::GetFallbackDockEdge() const {
  switch (GetTaskbarDockEdge()) {
    case DockEdge::Right:
      return DockEdge::Left;
    case DockEdge::Left:
    case DockEdge::Top:
    case DockEdge::Bottom:
    case DockEdge::None:
    default:
      return DockEdge::Right;
  }
}

App::DockEdge App::NormalizeDockEdge(DockEdge edge) const {
  if (edge == DockEdge::None) return edge;
  if (edge == GetTaskbarDockEdge()) {
    return GetFallbackDockEdge();
  }
  return edge;
}

bool App::IsDocked() const {
  switch (NormalizeDockEdge(static_cast<DockEdge>(state_.dockEdge))) {
    case DockEdge::Left:
    case DockEdge::Right:
    case DockEdge::Top:
    case DockEdge::Bottom:
      return true;
    case DockEdge::None:
    default:
      return false;
  }
}

void App::UpdateDockEdgeFromRect(RECT& rect) {
  RECT work = GetWorkArea();
  const int width = rect.right - rect.left;
  const int height = rect.bottom - rect.top;
  const int leftDist = std::abs(rect.left - work.left);
  const int rightDist = std::abs(work.right - rect.right);
  const int topDist = std::abs(rect.top - work.top);
  const int bottomDist = std::abs(work.bottom - rect.bottom);
  const DockEdge blocked = GetTaskbarDockEdge();
  const int disabled = 0x3fffffff;
  const int best = std::min({
    blocked == DockEdge::Left ? disabled : leftDist,
    blocked == DockEdge::Right ? disabled : rightDist,
    blocked == DockEdge::Top ? disabled : topDist,
    blocked == DockEdge::Bottom ? disabled : bottomDist
  });

  DockEdge edge = DockEdge::None;
  if (best <= kSnapThresholdPx) {
    if (blocked != DockEdge::Left && best == leftDist) edge = DockEdge::Left;
    else if (blocked != DockEdge::Right && best == rightDist) edge = DockEdge::Right;
    else if (blocked != DockEdge::Top && best == topDist) edge = DockEdge::Top;
    else if (blocked != DockEdge::Bottom && best == bottomDist) edge = DockEdge::Bottom;
  }

  state_.dockEdge = static_cast<int>(edge);
  if (edge == DockEdge::Left) {
    rect.left = work.left;
    rect.right = rect.left + width;
    rect.top = std::max(work.top, std::min(rect.top, work.bottom - height));
    rect.bottom = rect.top + height;
  } else if (edge == DockEdge::Right) {
    rect.left = work.right - width;
    rect.right = work.right;
    rect.top = std::max(work.top, std::min(rect.top, work.bottom - height));
    rect.bottom = rect.top + height;
  } else if (edge == DockEdge::Top) {
    rect.top = work.top;
    rect.bottom = rect.top + height;
    rect.left = std::max(work.left, std::min(rect.left, work.right - width));
    rect.right = rect.left + width;
  } else if (edge == DockEdge::Bottom) {
    rect.top = work.bottom - height;
    rect.bottom = work.bottom;
    rect.left = std::max(work.left, std::min(rect.left, work.right - width));
    rect.right = rect.left + width;
  } else {
    rect.left = std::max(work.left, std::min(rect.left, work.right - width));
    rect.top = std::max(work.top, std::min(rect.top, work.bottom - height));
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;
  }
}

RECT App::GetExpandedRect() const {
  RECT work = GetWorkArea();
  RECT rect{ state_.x, state_.y, state_.x + std::max(300, state_.w), state_.y + std::max(280, state_.h) };
  const int width = rect.right - rect.left;
  const int height = rect.bottom - rect.top;
  const DockEdge edge = NormalizeDockEdge(static_cast<DockEdge>(state_.dockEdge));

  if (edge == DockEdge::Left) {
    rect.left = work.left;
    rect.right = rect.left + width;
    rect.top = std::max(work.top, std::min(rect.top, work.bottom - height));
    rect.bottom = rect.top + height;
  } else if (edge == DockEdge::Right) {
    rect.left = work.right - width;
    rect.right = work.right;
    rect.top = std::max(work.top, std::min(rect.top, work.bottom - height));
    rect.bottom = rect.top + height;
  } else if (edge == DockEdge::Top) {
    rect.top = work.top;
    rect.bottom = rect.top + height;
    rect.left = std::max(work.left, std::min(rect.left, work.right - width));
    rect.right = rect.left + width;
  } else if (edge == DockEdge::Bottom) {
    rect.top = work.bottom - height;
    rect.bottom = work.bottom;
    rect.left = std::max(work.left, std::min(rect.left, work.right - width));
    rect.right = rect.left + width;
  } else {
    rect.left = std::max(work.left, std::min(rect.left, work.right - width));
    rect.top = std::max(work.top, std::min(rect.top, work.bottom - height));
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;
  }
  return rect;
}

RECT App::GetCollapsedRect() const {
  RECT rect = GetExpandedRect();
  const int width = rect.right - rect.left;
  const int height = rect.bottom - rect.top;
  switch (NormalizeDockEdge(static_cast<DockEdge>(state_.dockEdge))) {
    case DockEdge::Left:
      rect.left -= width;
      rect.right = rect.left + width;
      break;
    case DockEdge::Right:
      rect.left += width;
      rect.right = rect.left + width;
      break;
    case DockEdge::Top:
      rect.top -= height;
      rect.bottom = rect.top + height;
      break;
    case DockEdge::Bottom:
      rect.top += height;
      rect.bottom = rect.top + height;
      break;
    case DockEdge::None:
    default:
      break;
  }
  return rect;
}

RECT App::GetHotZoneRect() const {
  RECT work = GetWorkArea();
  switch (NormalizeDockEdge(static_cast<DockEdge>(state_.dockEdge))) {
    case DockEdge::Left:
      return RECT{ work.left, work.top, work.left + kHotZoneThicknessPx, work.bottom };
    case DockEdge::Right:
      return RECT{ work.right - kHotZoneThicknessPx, work.top, work.right, work.bottom };
    case DockEdge::Top:
      return RECT{ work.left, work.top, work.right, work.top + kHotZoneThicknessPx };
    case DockEdge::Bottom:
      return RECT{ work.left, work.bottom - kHotZoneThicknessPx, work.right, work.bottom };
    case DockEdge::None:
    default:
      return RECT{ 0, 0, 0, 0 };
  }
}

void App::SyncHotZone() {
  if (!hotZone_) return;
  const bool show = IsDocked() && !trayHidden_ && !state_.expanded && !animation_.active;
  hotZoneVisible_ = show;
  if (!show) {
    ShowWindow(hotZone_, SW_HIDE);
    return;
  }

  RECT rect = GetHotZoneRect();
  SetWindowPos(hotZone_, HWND_TOPMOST,
    rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
    SWP_SHOWWINDOW | SWP_NOACTIVATE);
}

void App::CommitVisibleRect(const RECT& rect, bool detectDock) {
  RECT normalized = rect;
  if (detectDock) {
    UpdateDockEdgeFromRect(normalized);
  } else if (IsDocked()) {
    RECT copy = normalized;
    UpdateDockEdgeFromRect(copy);
    normalized = copy;
  }

  state_.x = normalized.left;
  state_.y = normalized.top;
  state_.w = normalized.right - normalized.left;
  state_.h = normalized.bottom - normalized.top;
  SaveState();
}

void App::RequestExpand(bool activate) {
  if (animation_.active && animation_.expand) {
    if (activate) animation_.activateOnFinish = true;
    return;
  }

  trayHidden_ = false;
  if (!IsDocked()) {
    state_.expanded = true;
    ShowMainWindow(activate);
    SyncHotZone();
    SaveState();
    return;
  }

  if (state_.expanded && IsWindowVisible(hwnd_)) {
    ShowMainWindow(activate);
    return;
  }

  RECT collapsedRect = GetCollapsedRect();
  suppressWindowTracking_ = true;
  SetWindowPos(hwnd_, HWND_TOPMOST, collapsedRect.left, collapsedRect.top,
    collapsedRect.right - collapsedRect.left, collapsedRect.bottom - collapsedRect.top,
    SWP_NOACTIVATE | SWP_SHOWWINDOW);
  suppressWindowTracking_ = false;
  ShowMainWindow(false);
  BeginAnimation(true, activate);
}

void App::RequestCollapse() {
  if (!IsDocked() || trayHidden_ || !state_.expanded || animation_.active) return;
  BeginAnimation(false, false);
}

void App::BeginAnimation(bool expand, bool activateOnFinish) {
  if (animation_.active && animation_.expand == expand) {
    if (activateOnFinish) animation_.activateOnFinish = true;
    return;
  }
  KillTimer(hwnd_, kAnimationTimerId);
  animation_.active = true;
  animation_.expand = expand;
  animation_.activateOnFinish = activateOnFinish;
  GetWindowRect(hwnd_, &animation_.from);
  animation_.to = expand ? GetExpandedRect() : GetCollapsedRect();
  animation_.startTick = GetTickCount64();
  animation_.durationMs = expand ? kExpandDurationMs : kCollapseDurationMs;
  suppressWindowTracking_ = true;
  SetTimer(hwnd_, kAnimationTimerId, kAnimationTickMs, nullptr);
}

void App::TickAnimation() {
  if (!animation_.active) {
    KillTimer(hwnd_, kAnimationTimerId);
    return;
  }

  const ULONGLONG now = GetTickCount64();
  const double t = std::clamp(static_cast<double>(now - animation_.startTick) / std::max(1, animation_.durationMs), 0.0, 1.0);
  const double eased = 1.0 - (1.0 - t) * (1.0 - t);
  auto lerp = [eased](int a, int b) {
    return static_cast<int>(a + (b - a) * eased);
  };

  RECT rect{};
  rect.left = lerp(animation_.from.left, animation_.to.left);
  rect.top = lerp(animation_.from.top, animation_.to.top);
  rect.right = lerp(animation_.from.right, animation_.to.right);
  rect.bottom = lerp(animation_.from.bottom, animation_.to.bottom);

  SetWindowPos(hwnd_, HWND_TOPMOST, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
    SWP_NOACTIVATE | SWP_NOZORDER);

  if (t >= 1.0) {
    FinishAnimation();
  }
}

void App::FinishAnimation() {
  KillTimer(hwnd_, kAnimationTimerId);
  animation_.active = false;
  suppressWindowTracking_ = false;

  if (animation_.expand) {
    state_.expanded = true;
    CommitVisibleRect(animation_.to, false);
    SetTaskbarVisible(true);
    ShowMainWindow(animation_.activateOnFinish);
    if (animation_.activateOnFinish) {
      SetForegroundWindow(hwnd_);
    }
    lastPointerInsideTick_ = GetTickCount64();
  } else {
    state_.expanded = false;
    SetTaskbarVisible(false);
    SaveState();
  }
  SyncHotZone();
}

bool App::IsPointerInsideRect(const RECT& rect) const {
  POINT pt{};
  GetCursorPos(&pt);
  return PtInRect(&rect, pt) != FALSE;
}

void App::PollAutoHide() {
  if (trayHidden_ || inMoveSize_ || animation_.active || !state_.expanded || !IsDocked() || !IsWindowVisible(hwnd_)) {
    return;
  }

  RECT mainRect{};
  GetWindowRect(hwnd_, &mainRect);
  const ULONGLONG now = GetTickCount64();
  if (IsPointerInsideRect(mainRect)) {
    lastPointerInsideTick_ = now;
    return;
  }

  if (now - lastPointerInsideTick_ >= kAutoHideDelayMs) {
    RequestCollapse();
  }
}

void App::ApplySavedGeometry() {
  state_.version = 4;
  if (state_.w <= 0) state_.w = 420;
  if (state_.h <= 0) state_.h = 640;
  state_.dockEdge = static_cast<int>(NormalizeDockEdge(static_cast<DockEdge>(state_.dockEdge)));
  RECT rect = GetExpandedRect();
  CommitVisibleRect(rect, true);
  RECT target = state_.expanded && !trayHidden_ ? GetExpandedRect() : GetCollapsedRect();
  suppressWindowTracking_ = true;
  SetWindowPos(hwnd_, HWND_TOPMOST, target.left, target.top, target.right - target.left, target.bottom - target.top,
    SWP_NOACTIVATE | SWP_SHOWWINDOW);
  suppressWindowTracking_ = false;
  if (!state_.expanded) {
    SetTaskbarVisible(false);
  }
  SyncHotZone();
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
  SendMessageW(listBox_, WM_SETFONT, reinterpret_cast<WPARAM>(fontBody_), TRUE);
  SetWindowLongPtrW(listBox_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  originalListBoxProc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(listBox_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(App::ListBoxProc)));
  CreateToolbarButtons();
}

void App::DestroyToolbarButtons() {
  for (ToolbarButtonConfig& button : toolbarButtons_) {
    if (button.hwnd) {
      DestroyWindow(button.hwnd);
      button.hwnd = nullptr;
    }
  }
  if (toolbarTooltip_) {
    DestroyWindow(toolbarTooltip_);
    toolbarTooltip_ = nullptr;
  }
}

void App::CreateToolbarButtons() {
  DestroyToolbarButtons();
  if (toolbarButtons_.empty()) return;

  toolbarTooltip_ = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
    WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
    hwnd_, nullptr, instance_, nullptr);
  if (toolbarTooltip_) {
    SetWindowPos(toolbarTooltip_, HWND_TOPMOST, 0, 0, 0, 0,
      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }

  for (size_t i = 0; i < toolbarButtons_.size(); ++i) {
    ToolbarButtonConfig& button = toolbarButtons_[i];
    button.hwnd = CreateWindowExW(
      0,
      L"BUTTON",
      button.title.c_str(),
      WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
      0, 0, 0, 0,
      hwnd_,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolbarButtonBaseId + i)),
      instance_,
      nullptr);

    if (toolbarTooltip_ && button.hwnd) {
      TOOLINFOW ti{};
      ti.cbSize = sizeof(ti);
      ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
      ti.hwnd = hwnd_;
      ti.uId = reinterpret_cast<UINT_PTR>(button.hwnd);
      ti.lpszText = const_cast<LPWSTR>(button.title.c_str());
      SendMessageW(toolbarTooltip_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    }
  }
}

void App::CreateFonts() {
  const int dpi = GetDpiForWindow(hwnd_ ? hwnd_ : GetDesktopWindow());
  auto scale = [&](int points) {
    return -MulDiv(points, dpi, 72);
  };

  fontBody_ = CreateFontW(
    scale(8), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
    VARIABLE_PITCH, L"Segoe UI");
  fontGroup_ = CreateFontW(
    scale(10), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
    VARIABLE_PITCH, L"Segoe UI");
  fontMeta_ = CreateFontW(
    scale(9), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
    VARIABLE_PITCH, L"Segoe UI");
  fontSymbol_ = CreateFontW(
    scale(11), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
    DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
    VARIABLE_PITCH, L"Segoe UI Symbol");
}

void App::DestroyFonts() {
  if (fontBody_) DeleteObject(fontBody_);
  if (fontGroup_) DeleteObject(fontGroup_);
  if (fontMeta_) DeleteObject(fontMeta_);
  if (fontSymbol_) DeleteObject(fontSymbol_);
  fontBody_ = nullptr;
  fontGroup_ = nullptr;
  fontMeta_ = nullptr;
  fontSymbol_ = nullptr;
}

void App::LayoutControls() {
  RECT rc{};
  GetClientRect(hwnd_, &rc);
  const int toolbarHeight = toolbarButtons_.empty() ? 0 : kToolbarHeightPx;
  int buttonX = kToolbarPaddingPx;
  for (ToolbarButtonConfig& button : toolbarButtons_) {
    if (!button.hwnd) continue;
    MoveWindow(button.hwnd, buttonX, kToolbarPaddingPx,
      kToolbarButtonSizePx, kToolbarButtonSizePx, TRUE);
    buttonX += kToolbarButtonSizePx + kToolbarPaddingPx;
  }
  if (listBox_) {
    const int listWidth = static_cast<int>(rc.right - rc.left);
    const int listHeight = std::max(0, static_cast<int>(rc.bottom - rc.top) - toolbarHeight);
    MoveWindow(listBox_, 0, toolbarHeight, listWidth, listHeight, TRUE);
    RedrawWindow(listBox_, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
  }
}

void App::RefreshNotes(const std::unordered_map<std::wstring, bool>* expandedStateByGroupId) {
  std::unordered_map<std::wstring, bool> showAllStateByGroupId;
  const int oldCount = static_cast<int>(std::min(notesConfig_.groups.size(), showAllGroups_.size()));
  for (int i = 0; i < oldCount; ++i) {
    showAllStateByGroupId[notesConfig_.groups[i].id] = showAllGroups_[i];
  }

  notesByGroup_.clear();
  groupStates_.clear();
  expandedGroups_.clear();
  showAllGroups_.clear();
  globalStatusMessage_.clear();
  if (notesConfig_.groups.empty()) {
    globalStatusMessage_ = L"No note groups configured. Add [note_group.<id>] sections to config.ini.";
  }
  for (const NoteGroupConfig& group : notesConfig_.groups) {
    std::vector<NoteFile> files;
    NoteGroupLoadState state = NoteGroupLoadState::Ok;
    bool showAll = false;
    if (auto it = showAllStateByGroupId.find(group.id); it != showAllStateByGroupId.end()) {
      showAll = it->second;
    }
    LoadNoteFiles(group, files, &state, showAll);
    notesByGroup_.push_back(std::move(files));
    groupStates_.push_back(state);
    bool expanded = group.expanded;
    if (expandedStateByGroupId) {
      if (auto it = expandedStateByGroupId->find(group.id); it != expandedStateByGroupId->end()) {
        expanded = it->second;
      }
    }
    expandedGroups_.push_back(expanded);
    showAllGroups_.push_back(showAll);
  }
  RebuildVisibleRows();
}

void App::ReloadConfigAndRefreshNotes() {
  std::unordered_map<std::wstring, bool> expandedStateByGroupId;
  const int count = static_cast<int>(std::min(notesConfig_.groups.size(), expandedGroups_.size()));
  for (int i = 0; i < count; ++i) {
    expandedStateByGroupId[notesConfig_.groups[i].id] = expandedGroups_[i];
  }

  LoadConfig();
  RefreshNotes(&expandedStateByGroupId);
}

void App::RefreshGroup(int groupIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(notesConfig_.groups.size())) return;
  NoteGroupLoadState state = NoteGroupLoadState::Ok;
  bool showAll = groupIndex < static_cast<int>(showAllGroups_.size()) && showAllGroups_[groupIndex];
  LoadNoteFiles(notesConfig_.groups[groupIndex], notesByGroup_[groupIndex], &state, showAll);
  groupStates_[groupIndex] = state;
  RebuildVisibleRows();
}

void App::RebuildVisibleRows() {
  int topIndex = 0;
  if (listBox_) {
    LRESULT result = SendMessageW(listBox_, LB_GETTOPINDEX, 0, 0);
    if (result != LB_ERR) topIndex = static_cast<int>(result);
  }

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
  if (!visibleRows_.empty()) {
    topIndex = std::clamp(topIndex, 0, static_cast<int>(visibleRows_.size()) - 1);
    SendMessageW(listBox_, LB_SETTOPINDEX, static_cast<WPARAM>(topIndex), 0);
    if (currentRowIndex_ >= static_cast<int>(visibleRows_.size())) {
      currentRowIndex_ = static_cast<int>(visibleRows_.size()) - 1;
    }
  } else {
    currentRowIndex_ = -1;
  }
  SendMessageW(listBox_, WM_SETREDRAW, TRUE, 0);
  InvalidateList();
  UpdateToolbarButtons();
}

void App::InvalidateList() {
  if (listBox_) {
    RedrawWindow(listBox_, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
  }
}

RECT App::GetRowRect(int index) const {
  RECT rc{};
  SendMessageW(listBox_, LB_GETITEMRECT, index, reinterpret_cast<LPARAM>(&rc));
  return rc;
}

RECT App::GetGroupToggleRect(const RECT& rowRect) const {
  return RECT{ rowRect.left + 4, rowRect.top + 4, rowRect.left + 24, rowRect.bottom - 4 };
}

RECT App::GetGroupShowAllRect(const RECT& rowRect) const {
  return RECT{
    rowRect.right - kGroupClipboardWidth - kGroupAddWidth - 12,
    rowRect.top + 4,
    rowRect.right - kGroupClipboardWidth - 12,
    rowRect.bottom - 4
  };
}

RECT App::GetGroupOpenRect(const RECT& rowRect) const {
  return RECT{ rowRect.right - kGroupClipboardWidth - 8, rowRect.top + 4, rowRect.right - 8, rowRect.bottom - 4 };
}

RECT App::GetGroupAddRect(const RECT& rowRect) const {
  return RECT{
    rowRect.right - kGroupClipboardWidth - kGroupAddWidth - 12,
    rowRect.top + 4,
    rowRect.right - kGroupClipboardWidth - 12,
    rowRect.bottom - 4
  };
}

RECT App::GetGroupClipboardRect(const RECT& rowRect) const {
  return GetGroupOpenRect(rowRect);
}

void App::DrawListItem(const DRAWITEMSTRUCT* dis) {
  if (dis->itemID == static_cast<UINT>(-1) || dis->itemID >= visibleRows_.size()) return;

  const VisibleRow& row = visibleRows_[dis->itemID];
  HDC dc = dis->hDC;
  RECT rc = dis->rcItem;
  HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, fontBody_));

  HBRUSH bg = CreateSolidBrush(row.type == VisibleRow::Type::Group ? RGB(240, 244, 248) : RGB(255, 255, 255));
  FillRect(dc, &rc, bg);
  DeleteObject(bg);

  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(32, 32, 32));

  if (row.type == VisibleRow::Type::GlobalMessage) {
    RECT textRc = rc;
    textRc.left += 12;
    SelectObject(dc, fontMeta_);
    SetTextColor(dc, RGB(112, 112, 112));
    DrawTextW(dc, globalStatusMessage_.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
  } else if (row.type == VisibleRow::Type::Group) {
    const NoteGroupConfig& group = notesConfig_.groups[row.groupIndex];
    const bool supportsNewNotes = group.type == NoteGroupType::Directory;
    const bool supportsShowAll = group.type == NoteGroupType::TextLines && group.maxItems > 0 &&
      row.groupIndex < static_cast<int>(showAllGroups_.size());
    const bool showAllEnabled = supportsShowAll && showAllGroups_[row.groupIndex];
    NoteFile sourceFile{};
    const bool supportsOpen = group.type == NoteGroupType::TextLines &&
      !group.defaultFileAction.empty() &&
      BuildGroupSourceFile(group, sourceFile);
    RECT toggleRc = GetGroupToggleRect(rc);
    RECT showAllRc = GetGroupShowAllRect(rc);
    RECT openRc = GetGroupOpenRect(rc);
    RECT addRc = GetGroupAddRect(rc);
    RECT clipboardRc = GetGroupClipboardRect(rc);
    RECT textRc = rc;
    textRc.left += kGroupIndent + 14;
    if (supportsNewNotes) {
      textRc.right = addRc.left - 8;
    } else if (supportsShowAll) {
      textRc.right = showAllRc.left - 8;
    } else if (supportsOpen) {
      textRc.right = openRc.left - 8;
    } else {
      textRc.right = rc.right - 8;
    }

    SelectObject(dc, fontSymbol_);
    DrawTextW(dc, expandedGroups_[row.groupIndex] ? L"v" : L">", -1, &toggleRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    SelectObject(dc, fontGroup_);
    DrawTextW(dc, group.title.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    if (supportsShowAll) {
      SelectObject(dc, fontMeta_);
      SetTextColor(dc, showAllEnabled ? RGB(96, 104, 116) : RGB(64, 98, 160));
      DrawTextW(dc, L"...", -1, &showAllRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }
    if (supportsOpen) {
      SelectObject(dc, fontMeta_);
      SetTextColor(dc, RGB(64, 98, 160));
      DrawTextW(dc, L"O", -1, &openRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }
    if (supportsNewNotes) {
      SelectObject(dc, fontMeta_);
      SetTextColor(dc, RGB(64, 98, 160));
      DrawTextW(dc, L"+", -1, &addRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
      DrawTextW(dc, L"P", -1, &clipboardRc, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }
  } else if (row.type == VisibleRow::Type::GroupMessage) {
    RECT textRc = rc;
    textRc.left += kFileIndent;
    textRc.right -= 8;
    SelectObject(dc, fontMeta_);
    SetTextColor(dc, RGB(112, 112, 112));
    std::wstring message = GetGroupStatusMessage(groupStates_[row.groupIndex], notesConfig_.groups[row.groupIndex]);
    DrawTextW(dc, message.c_str(), -1, &textRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
  } else {
    const NoteFile& file = notesByGroup_[row.groupIndex][row.fileIndex];
    const NoteGroupConfig& group = notesConfig_.groups[row.groupIndex];
    RECT nameRc = rc;
    nameRc.left += kFileIndent;
    RECT timeRc = rc;
    timeRc.right -= 8;

    SelectObject(dc, fontBody_);
    const std::wstring& displayName = file.displayName.empty()
      ? (group.showExtensions ? file.name : file.stem)
      : file.displayName;
    if (group.type == NoteGroupType::TextLines) {
      nameRc.right = timeRc.right - 32;
      MarkdownCheckbox checkbox = ParseMarkdownCheckbox(displayName);
      if (checkbox.state == MarkdownCheckbox::State::None) {
        DrawTextW(dc, displayName.c_str(), -1, &nameRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_EXPANDTABS);
      } else {
        const int indentWidth = MeasureTextWidth(dc, ExpandTabsForDisplay(checkbox.indent));
        RECT boxRc{};
        boxRc.left = nameRc.left + indentWidth;
        boxRc.top = rc.top + ((rc.bottom - rc.top) - 13) / 2;
        boxRc.right = boxRc.left + 13;
        boxRc.bottom = boxRc.top + 13;
        UINT checkboxState = DFCS_BUTTONCHECK;
        if (checkbox.state == MarkdownCheckbox::State::Checked) checkboxState |= DFCS_CHECKED;
        DrawFrameControl(dc, &boxRc, DFC_BUTTON, checkboxState);

        RECT labelRc = nameRc;
        labelRc.left = boxRc.right + 6;
        DrawTextW(dc, checkbox.label.c_str(), -1, &labelRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
      }
      std::wstring lineText = L"#" + std::to_wstring(file.lineNumber);
      timeRc.left = nameRc.right + 8;
      SelectObject(dc, fontMeta_);
      SetTextColor(dc, RGB(96, 104, 116));
      DrawTextW(dc, lineText.c_str(), -1, &timeRc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    } else {
      constexpr int kFileMetaGap = 8;
      constexpr int kMinNameWidth = 120;
      const int contentRight = rc.right - 8;
      const int totalWidth = contentRight - nameRc.left;
      const int availableTimeWidth = std::max(0, totalWidth - kMinNameWidth - kFileMetaGap);
      std::wstring timeText = FormatAdaptiveFileTime(dc, file.modifiedTime, availableTimeWidth);
      const int timeWidth = MeasureTextWidth(dc, timeText);
      if (!timeText.empty() && timeWidth > 0) {
        timeRc.left = contentRight - timeWidth;
        nameRc.right = timeRc.left - kFileMetaGap;
      } else {
        nameRc.right = contentRight;
        timeRc.left = timeRc.right;
      }

      DrawTextW(dc, displayName.c_str(), -1, &nameRc, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
      SelectObject(dc, fontMeta_);
      SetTextColor(dc, RGB(96, 104, 116));
      if (!timeText.empty()) {
        DrawTextW(dc, timeText.c_str(), -1, &timeRc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
      }
    }
  }

  SelectObject(dc, oldFont);
}

void App::DrawToolbarButton(const DRAWITEMSTRUCT* dis) {
  const size_t index = static_cast<size_t>(dis->CtlID - kToolbarButtonBaseId);
  if (index >= toolbarButtons_.size()) return;

  const ToolbarButtonConfig& button = toolbarButtons_[index];
  HDC dc = dis->hDC;
  RECT rc = dis->rcItem;

  const bool selected = (dis->itemState & ODS_SELECTED) != 0;
  const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
  const COLORREF bg = selected ? RGB(228, 234, 242) : RGB(245, 247, 250);
  const COLORREF border = selected ? RGB(120, 132, 148) : RGB(214, 220, 228);
  const COLORREF iconColor = disabled ? RGB(164, 170, 178) : RGB(52, 76, 112);

  HBRUSH bgBrush = CreateSolidBrush(bg);
  FillRect(dc, &rc, bgBrush);
  DeleteObject(bgBrush);

  HPEN borderPen = CreatePen(PS_SOLID, 1, border);
  HGDIOBJ oldPen = SelectObject(dc, borderPen);
  HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
  Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
  SelectObject(dc, oldBrush);
  SelectObject(dc, oldPen);
  DeleteObject(borderPen);

  RECT iconRc = rc;
  iconRc.left += 3;
  iconRc.top += 3;
  iconRc.right -= 3;
  iconRc.bottom -= 3;

  if (button.icon == L"touch") {
    DrawTouchToolbarIcon(dc, iconRc, iconColor);
  } else if (button.icon == L"proxy") {
    DrawProxyToolbarIcon(dc, iconRc, iconColor);
  }

  if (dis->itemState & ODS_FOCUS) {
    RECT focusRc = rc;
    InflateRect(&focusRc, -3, -3);
    DrawFocusRect(dc, &focusRc);
  }
}

int App::GetCurrentRowIndex() const {
  if (currentRowIndex_ >= 0 && currentRowIndex_ < static_cast<int>(visibleRows_.size())) {
    return currentRowIndex_;
  }
  if (!listBox_) return -1;
  LRESULT result = SendMessageW(listBox_, LB_GETCURSEL, 0, 0);
  if (result == LB_ERR) return -1;
  int index = static_cast<int>(result);
  return index >= 0 && index < static_cast<int>(visibleRows_.size()) ? index : -1;
}

bool App::ResolveToolbarContext(const ToolbarButtonConfig& button, int& groupIndex, NoteGroupConfig& group,
  NoteFile& file, const NoteFile*& filePtr) const {
  groupIndex = -1;
  file = NoteFile{};
  filePtr = nullptr;

  if (button.scope == ToolbarScope::Global) {
    group = NoteGroupConfig{};
    return true;
  }

  const int rowIndex = GetCurrentRowIndex();
  if (rowIndex < 0 || rowIndex >= static_cast<int>(visibleRows_.size())) return false;
  const VisibleRow& row = visibleRows_[rowIndex];
  if (row.type == VisibleRow::Type::GlobalMessage) return false;

  groupIndex = row.groupIndex;
  if (groupIndex < 0 || groupIndex >= static_cast<int>(notesConfig_.groups.size())) return false;
  group = notesConfig_.groups[groupIndex];

  if (button.scope == ToolbarScope::Group) {
    return true;
  }

  if (row.type == VisibleRow::Type::File) {
    file = notesByGroup_[row.groupIndex][row.fileIndex];
    filePtr = &file;
    return true;
  }

  if (group.type == NoteGroupType::TextLines && BuildGroupSourceFile(group, file)) {
    filePtr = &file;
    return true;
  }

  return false;
}

void App::UpdateToolbarButtons() {
  for (ToolbarButtonConfig& button : toolbarButtons_) {
    if (!button.hwnd) continue;
    if (button.scope == ToolbarScope::Global) {
      EnableWindow(button.hwnd, TRUE);
      continue;
    }

    int groupIndex = -1;
    NoteGroupConfig group{};
    NoteFile file{};
    const NoteFile* filePtr = nullptr;
    EnableWindow(button.hwnd, ResolveToolbarContext(button, groupIndex, group, file, filePtr) ? TRUE : FALSE);
  }
}

void App::RunToolbarButton(size_t index) {
  if (index >= toolbarButtons_.size()) return;
  const ToolbarButtonConfig& button = toolbarButtons_[index];

  if (button.command == L":refresh") {
    ReloadConfigAndRefreshNotes();
    return;
  }
  if (button.command == L":config") {
    OpenConfigFile();
    return;
  }

  int groupIndex = -1;
  NoteGroupConfig group{};
  NoteFile file{};
  const NoteFile* filePtr = nullptr;
  if (!ResolveToolbarContext(button, groupIndex, group, file, filePtr)) return;

  ActionConfig action{};
  action.id = button.id;
  action.title = button.title;
  action.command = button.command;
  action.target = filePtr ? ActionTarget::File : ActionTarget::Directory;

  std::wstring errorMessage;
  std::wstring command;
  const bool shouldWait = button.scope != ToolbarScope::Global;
  const bool ok = shouldWait
    ? ExecuteActionAndWait(action, group, filePtr, &errorMessage, &command)
    : ExecuteAction(action, group, filePtr, &errorMessage, &command);
  if (!ok) {
    std::wstring message = L"Failed to run toolbar command:\n" + button.title + L"\n\nCommand:\n" +
      command + L"\n\n" + errorMessage;
    MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }

  if (shouldWait && groupIndex >= 0) {
    RefreshGroup(groupIndex);
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
  currentRowIndex_ = index;
  UpdateToolbarButtons();

  const VisibleRow& row = visibleRows_[index];
  RECT rowRect = GetRowRect(index);
  if (row.type == VisibleRow::Type::Group) {
    const NoteGroupConfig& group = notesConfig_.groups[row.groupIndex];
    const bool supportsNewNotes = group.type == NoteGroupType::Directory;
    const bool supportsShowAll = group.type == NoteGroupType::TextLines && group.maxItems > 0 &&
      row.groupIndex < static_cast<int>(showAllGroups_.size());
    RECT showAllRc = GetGroupShowAllRect(rowRect);
    RECT openRc = GetGroupOpenRect(rowRect);
    RECT addRc = GetGroupAddRect(rowRect);
    RECT clipboardRc = GetGroupClipboardRect(rowRect);
    RECT toggleRc = GetGroupToggleRect(rowRect);
    if (supportsShowAll && PtInRect(&showAllRc, pt)) {
      ShowAllForGroup(row.groupIndex);
    } else if (group.type == NoteGroupType::TextLines && PtInRect(&openRc, pt)) {
      OpenGroupNote(row.groupIndex);
    } else if (supportsNewNotes && PtInRect(&addRc, pt)) {
      CreateNoteForGroup(row.groupIndex);
    } else if (supportsNewNotes && PtInRect(&clipboardRc, pt)) {
      CreateNoteFromClipboardForGroup(row.groupIndex);
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
  POINT screenPt = pt;
  ClientToScreen(listBox_, &screenPt);
  if (index < 0) {
    RunBlankMenu(screenPt);
    return;
  }

  currentRowIndex_ = index;
  UpdateToolbarButtons();

  const VisibleRow& row = visibleRows_[index];
  if (row.type == VisibleRow::Type::Group) {
    RunGroupMenu(row.groupIndex, screenPt);
  } else if (row.type == VisibleRow::Type::File) {
    RunFileMenu(row.groupIndex, row.fileIndex, screenPt);
  } else {
    RunBlankMenu(screenPt);
  }
}

void App::ToggleGroup(int groupIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(expandedGroups_.size())) return;
  expandedGroups_[groupIndex] = !expandedGroups_[groupIndex];
  RebuildVisibleRows();
}

void App::ShowAllForGroup(int groupIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(notesConfig_.groups.size())) return;
  if (notesConfig_.groups[groupIndex].type != NoteGroupType::TextLines) return;
  if (notesConfig_.groups[groupIndex].maxItems == 0) return;
  if (groupIndex >= static_cast<int>(showAllGroups_.size())) return;
  showAllGroups_[groupIndex] = !showAllGroups_[groupIndex];
  RefreshGroup(groupIndex);
}

void App::OpenGroupNote(int groupIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(notesConfig_.groups.size())) return;
  const NoteGroupConfig& group = notesConfig_.groups[groupIndex];
  if (group.type != NoteGroupType::TextLines || group.defaultFileAction.empty()) return;
  auto actionIt = notesConfig_.actions.find(group.defaultFileAction);
  if (actionIt == notesConfig_.actions.end()) return;
  NoteFile file{};
  if (!BuildGroupSourceFile(group, file)) return;
  if (!IsActionTargetCompatible(actionIt->second, &file)) return;
  std::wstring errorMessage;
  std::wstring command;
  if (!ExecuteAction(actionIt->second, group, &file, &errorMessage, &command)) {
    std::wstring message = L"Failed to run action:\n" + actionIt->second.title + L"\n\nCommand:\n" + command + L"\n\n" + errorMessage;
    MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
  }
}

void App::OpenConfigFile() {
  auto actionIt = notesConfig_.actions.find(L"Edit");
  if (actionIt == notesConfig_.actions.end()) {
    actionIt = notesConfig_.actions.find(L"edit");
  }
  if (actionIt == notesConfig_.actions.end()) {
    MessageBoxW(hwnd_, L"Opening config requires file_action.Edit.", L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }

  NoteFile configFile{};
  configFile.path = currentConfigPath_.empty() ? GetFallbackConfigPath() : currentConfigPath_;
  configFile.name = GetFileNameFromPath(configFile.path);
  configFile.stem = GetFileStemFromName(configFile.name);
  const size_t pos = configFile.path.find_last_of(L"\\/");
  configFile.dir = pos == std::wstring::npos ? L"" : configFile.path.substr(0, pos);
  configFile.displayName = configFile.name;

  NoteGroupConfig pseudoGroup{};
  pseudoGroup.id = L"config";
  pseudoGroup.title = L"Config";
  pseudoGroup.path = configFile.dir;

  std::wstring errorMessage;
  std::wstring command;
  if (!ExecuteAction(actionIt->second, pseudoGroup, &configFile, &errorMessage, &command)) {
    std::wstring message = L"Failed to run action:\n" + actionIt->second.title + L"\n\nCommand:\n" + command + L"\n\n" + errorMessage;
    MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
  }
}

void App::CreateNoteForGroup(int groupIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(notesConfig_.groups.size())) return;
  const NoteGroupConfig& group = notesConfig_.groups[groupIndex];
  if (group.type != NoteGroupType::Directory) return;
  auto actionIt = FindNewNoteEditAction(notesConfig_);
  if (actionIt == notesConfig_.actions.end()) {
    MessageBoxW(hwnd_, L"New notes require file_action.Edit.", L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }

  std::wstring draftPath;
  std::wstring errorMessage;
  if (!CreateTempNoteForGroup(group, draftPath, &errorMessage)) {
    MessageBoxW(hwnd_, errorMessage.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }

  NoteFile draft{};
  draft.path = draftPath;
  auto pos = draftPath.find_last_of(L"\\/");
  draft.dir = pos == std::wstring::npos ? L"" : draftPath.substr(0, pos);
  draft.name = pos == std::wstring::npos ? draftPath : draftPath.substr(pos + 1);
  draft.stem = draft.name;
  auto dot = draft.stem.find_last_of(L'.');
  if (dot != std::wstring::npos) draft.stem = draft.stem.substr(0, dot);

  FILETIME beforeWrite{};
  ULONGLONG beforeSize = 0;
  GetFileSnapshot(draftPath, beforeWrite, beforeSize);

  std::wstring command;
  if (!ExecuteActionAndWait(actionIt->second, group, &draft, &errorMessage, &command)) {
    DeleteFileW(draftPath.c_str());
    std::wstring message = L"Failed to run action:\n" + actionIt->second.title + L"\n\nCommand:\n" + command + L"\n\n" + errorMessage;
    MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }

  FILETIME afterWrite{};
  ULONGLONG afterSize = 0;
  bool existsAfter = GetFileSnapshot(draftPath, afterWrite, afterSize);
  bool changed = existsAfter &&
    (CompareFileTime(&afterWrite, &beforeWrite) != 0 || afterSize != beforeSize);

  if (!changed) {
    if (existsAfter) DeleteFileW(draftPath.c_str());
    return;
  }

  std::wstring finalPath;
  if (!MoveTempNoteIntoGroup(group, draftPath, finalPath, &errorMessage)) {
    std::wstring message = L"Edited note was not saved into the target group.\n\n" + errorMessage;
    MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }

  RefreshGroup(groupIndex);
}

void App::CreateNoteFromClipboardForGroup(int groupIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(notesConfig_.groups.size())) return;
  const NoteGroupConfig& group = notesConfig_.groups[groupIndex];
  if (group.type != NoteGroupType::Directory) return;

  std::wstring clipboardText;
  std::wstring errorMessage;
  if (!ReadClipboardText(hwnd_, clipboardText, &errorMessage)) {
    MessageBoxW(hwnd_, errorMessage.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }

  std::wstring draftPath;
  if (!CreateTempNoteForGroup(group, draftPath, &errorMessage)) {
    MessageBoxW(hwnd_, errorMessage.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }

  if (!WriteUtf8TextFile(draftPath, clipboardText, &errorMessage)) {
    DeleteFileW(draftPath.c_str());
    MessageBoxW(hwnd_, errorMessage.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }

  std::wstring finalPath;
  if (!MoveTempNoteIntoGroup(group, draftPath, finalPath, &errorMessage)) {
    DeleteFileW(draftPath.c_str());
    std::wstring message = L"Clipboard note was not saved into the target group.\n\n" + errorMessage;
    MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }

  RefreshGroup(groupIndex);
}

void App::OpenFileNote(int groupIndex, int fileIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(notesConfig_.groups.size())) return;
  const NoteGroupConfig& group = notesConfig_.groups[groupIndex];
  if (group.defaultFileAction.empty()) return;
  auto actionIt = notesConfig_.actions.find(group.defaultFileAction);
  if (actionIt == notesConfig_.actions.end()) return;
  if (!IsActionTargetCompatible(actionIt->second, &notesByGroup_[groupIndex][fileIndex])) return;
  std::wstring errorMessage;
  std::wstring command;
  if (!ExecuteAction(actionIt->second, group, &notesByGroup_[groupIndex][fileIndex], &errorMessage, &command)) {
    std::wstring message = L"Failed to run action:\n" + actionIt->second.title + L"\n\nCommand:\n" + command + L"\n\n" + errorMessage;
    MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
  }
}

void App::DeleteFileNote(int groupIndex, int fileIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(notesConfig_.groups.size())) return;
  if (fileIndex < 0 || fileIndex >= static_cast<int>(notesByGroup_[groupIndex].size())) return;
  const NoteGroupConfig& group = notesConfig_.groups[groupIndex];
  const NoteFile& file = notesByGroup_[groupIndex][fileIndex];

  if (!group.deleteCommand.empty()) {
    ActionConfig deleteAction{};
    deleteAction.id = L"Delete";
    deleteAction.title = L"Delete";
    deleteAction.command = group.deleteCommand;
    deleteAction.target = ActionTarget::File;

    std::wstring errorMessage;
    std::wstring command;
    if (!ExecuteActionAndWait(deleteAction, group, &file, &errorMessage, &command)) {
      std::wstring message = L"Failed to run delete command.\n\nCommand:\n" + command + L"\n\n" + errorMessage;
      MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
      return;
    }
    RefreshGroup(groupIndex);
    return;
  }

  std::wstring errorMessage;
  if (!MovePathToRecycleBin(file.path, &errorMessage)) {
    std::wstring message = L"Failed to delete file.\n\n" + errorMessage;
    MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }

  RefreshGroup(groupIndex);
}

void App::DeleteTextGroupSource(int groupIndex) {
  if (groupIndex < 0 || groupIndex >= static_cast<int>(notesConfig_.groups.size())) return;
  const NoteGroupConfig& group = notesConfig_.groups[groupIndex];
  if (group.type != NoteGroupType::TextLines) return;
  NoteFile sourceFile{};
  const bool hasSourceFile = BuildGroupSourceFile(group, sourceFile);

  if (hasSourceFile && !group.deleteCommand.empty()) {
    ActionConfig deleteAction{};
    deleteAction.id = L"Delete";
    deleteAction.title = L"Delete";
    deleteAction.command = group.deleteCommand;
    deleteAction.target = ActionTarget::File;

    std::wstring errorMessage;
    std::wstring command;
    if (!ExecuteActionAndWait(deleteAction, group, &sourceFile, &errorMessage, &command)) {
      std::wstring message = L"Failed to run delete command.\n\nCommand:\n" + command + L"\n\n" + errorMessage;
      MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
      return;
    }
    RefreshGroup(groupIndex);
    return;
  }

  std::wstring errorMessage;
  if (!MovePathToRecycleBin(group.path, &errorMessage)) {
    std::wstring message = L"Failed to delete file.\n\n" + errorMessage;
    MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    return;
  }

  RefreshGroup(groupIndex);
}

void App::RunGroupMenu(int groupIndex, POINT screenPt) {
  const NoteGroupConfig& group = notesConfig_.groups[groupIndex];
  const bool supportsNewNotes = group.type == NoteGroupType::Directory;
  NoteFile sourceFile{};
  const bool hasSourceFile = group.type == NoteGroupType::TextLines && BuildGroupSourceFile(group, sourceFile);
  HMENU menu = CreatePopupMenu();
  if (!menu) return;
  constexpr UINT kMenuNew = 4001;
  constexpr UINT kMenuNewFromClipboard = 4002;
  constexpr UINT kMenuRefresh = 4003;
  constexpr UINT kMenuConfig = 4004;
  constexpr UINT kMenuDelete = 4005;
  UINT nextActionId = 4100;

  if (supportsNewNotes) {
    AppendMenuW(menu, MF_STRING, kMenuNew, L"New Note");
    AppendMenuW(menu, MF_STRING, kMenuNewFromClipboard, L"New From Clipboard");
    if (!group.groupActions.empty()) {
      AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }

    std::unordered_map<UINT, std::wstring> actionIds;
    for (const std::wstring& actionId : group.groupActions) {
      auto it = notesConfig_.actions.find(actionId);
      if (it == notesConfig_.actions.end()) continue;
      if (!IsActionTargetCompatible(it->second, nullptr)) continue;
      AppendMenuW(menu, MF_STRING, nextActionId, it->second.title.c_str());
      actionIds[nextActionId] = actionId;
      ++nextActionId;
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuRefresh, L"Refresh");
    AppendMenuW(menu, MF_STRING, kMenuConfig, L"Config");

    SetForegroundWindow(hwnd_);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
      screenPt.x, screenPt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);

    if (cmd == kMenuNew) {
      CreateNoteForGroup(groupIndex);
    } else if (cmd == kMenuNewFromClipboard) {
      CreateNoteFromClipboardForGroup(groupIndex);
    } else if (cmd == kMenuRefresh) {
      ReloadConfigAndRefreshNotes();
    } else if (cmd == kMenuConfig) {
      OpenConfigFile();
    } else if (auto it = actionIds.find(cmd); it != actionIds.end()) {
      std::wstring errorMessage;
      std::wstring command;
      if (!ExecuteAction(notesConfig_.actions[it->second], group, nullptr, &errorMessage, &command)) {
        std::wstring message = L"Failed to run action:\n" + notesConfig_.actions[it->second].title + L"\n\nCommand:\n" + command + L"\n\n" + errorMessage;
        MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
      }
    }
    return;
  }

  std::unordered_map<UINT, std::wstring> actionIds;
  if (hasSourceFile && !group.defaultFileAction.empty()) {
    auto it = notesConfig_.actions.find(group.defaultFileAction);
    if (it != notesConfig_.actions.end() && IsActionTargetCompatible(it->second, &sourceFile)) {
      AppendMenuW(menu, MF_STRING, nextActionId, it->second.title.c_str());
      actionIds[nextActionId] = group.defaultFileAction;
      ++nextActionId;
    }
  }
  for (const std::wstring& actionId : group.fileActions) {
    if (actionId == group.defaultFileAction) continue;
    auto it = notesConfig_.actions.find(actionId);
    if (it == notesConfig_.actions.end()) continue;
    if (!hasSourceFile || !IsActionTargetCompatible(it->second, &sourceFile)) continue;
    AppendMenuW(menu, MF_STRING, nextActionId, it->second.title.c_str());
    actionIds[nextActionId] = actionId;
    ++nextActionId;
  }
  if (hasSourceFile) AppendMenuW(menu, MF_STRING, kMenuDelete, L"Delete");
  if (!actionIds.empty() || hasSourceFile) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuRefresh, L"Refresh");
  AppendMenuW(menu, MF_STRING, kMenuConfig, L"Config");

  SetForegroundWindow(hwnd_);
  UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
    screenPt.x, screenPt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);

  if (cmd == kMenuDelete) {
    DeleteTextGroupSource(groupIndex);
  } else if (cmd == kMenuRefresh) {
    ReloadConfigAndRefreshNotes();
  } else if (cmd == kMenuConfig) {
    OpenConfigFile();
  } else if (auto it = actionIds.find(cmd); it != actionIds.end()) {
    std::wstring errorMessage;
    std::wstring command;
    if (!ExecuteAction(notesConfig_.actions[it->second], group, &sourceFile, &errorMessage, &command)) {
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
  constexpr UINT kMenuRefresh = 5000;
  constexpr UINT kMenuConfig = 5001;
  constexpr UINT kMenuDelete = 5002;
  const bool canDelete = group.type == NoteGroupType::Directory;

  UINT nextActionId = 5100;
  std::unordered_map<UINT, std::wstring> actionIds;

  if (!group.defaultFileAction.empty()) {
    auto it = notesConfig_.actions.find(group.defaultFileAction);
    if (it != notesConfig_.actions.end()) {
      if (!IsActionTargetCompatible(it->second, &file)) {
        it = notesConfig_.actions.end();
      }
    }
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
    if (!IsActionTargetCompatible(it->second, &file)) continue;
    AppendMenuW(menu, MF_STRING, nextActionId, it->second.title.c_str());
    actionIds[nextActionId] = actionId;
    ++nextActionId;
  }
  if (canDelete) AppendMenuW(menu, MF_STRING, kMenuDelete, L"Delete");
  if (!actionIds.empty() || canDelete) AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuRefresh, L"Refresh");
  AppendMenuW(menu, MF_STRING, kMenuConfig, L"Config");

  SetForegroundWindow(hwnd_);
  UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
    screenPt.x, screenPt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);
  if (canDelete && cmd == kMenuDelete) {
    DeleteFileNote(groupIndex, fileIndex);
  } else if (cmd == kMenuRefresh) {
    ReloadConfigAndRefreshNotes();
  } else if (cmd == kMenuConfig) {
    OpenConfigFile();
  } else if (auto it = actionIds.find(cmd); it != actionIds.end()) {
    std::wstring errorMessage;
    std::wstring command;
    if (!ExecuteAction(notesConfig_.actions[it->second], group, &file, &errorMessage, &command)) {
      std::wstring message = L"Failed to run action:\n" + notesConfig_.actions[it->second].title + L"\n\nCommand:\n" + command + L"\n\n" + errorMessage;
      MessageBoxW(hwnd_, message.c_str(), L"MyBuddy", MB_OK | MB_ICONERROR);
    }
  }
}

void App::RunBlankMenu(POINT screenPt) {
  HMENU menu = CreatePopupMenu();
  if (!menu) return;
  constexpr UINT kMenuRefresh = 6001;
  constexpr UINT kMenuConfig = 6002;
  AppendMenuW(menu, MF_STRING, kMenuRefresh, L"Refresh");
  AppendMenuW(menu, MF_STRING, kMenuConfig, L"Config");

  SetForegroundWindow(hwnd_);
  UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN,
    screenPt.x, screenPt.y, 0, hwnd_, nullptr);
  DestroyMenu(menu);

  if (cmd == kMenuRefresh) {
    ReloadConfigAndRefreshNotes();
  } else if (cmd == kMenuConfig) {
    OpenConfigFile();
  }
}

std::wstring App::GetAppDataDir() const {
  PWSTR knownPath = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &knownPath))) {
    return L".\\";
  }
  std::wstring dir = EnsureTrailingSlash(knownPath);
  CoTaskMemFree(knownPath);
  dir += L"MyBuddy";
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
