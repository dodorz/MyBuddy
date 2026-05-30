#include "notes.h"

#include <algorithm>
#include <set>
#include <shellapi.h>

namespace {
std::vector<std::wstring> SplitList(const std::wstring& value) {
  std::vector<std::wstring> parts;
  std::wstring current;
  for (wchar_t ch : value) {
    if (ch == L';') {
      if (!current.empty()) {
        parts.push_back(current);
        current.clear();
      }
      continue;
    }
    current.push_back(ch);
  }
  if (!current.empty()) {
    parts.push_back(current);
  }
  return parts;
}

std::wstring ReadString(const std::wstring& path, const wchar_t* section, const wchar_t* key, const wchar_t* def = L"") {
  wchar_t buffer[4096];
  GetPrivateProfileStringW(section, key, def, buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
  return buffer;
}

int ReadInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int def) {
  return GetPrivateProfileIntW(section, key, def, path.c_str());
}

bool ReadBool(const std::wstring& path, const wchar_t* section, const wchar_t* key, bool def) {
  return ReadInt(path, section, key, def ? 1 : 0) != 0;
}

std::wstring GetFileStem(const std::wstring& name) {
  auto pos = name.find_last_of(L'.');
  if (pos == std::wstring::npos) return name;
  return name.substr(0, pos);
}

NoteSortBy ParseSortBy(const std::wstring& value, NoteSortBy fallback) {
  if (value == L"name") return NoteSortBy::Name;
  if (value == L"ctime") return NoteSortBy::CreatedTime;
  if (value == L"mtime") return NoteSortBy::ModifiedTime;
  return fallback;
}

SortOrder ParseSortOrder(const std::wstring& value, SortOrder fallback) {
  if (value == L"asc") return SortOrder::Asc;
  if (value == L"desc") return SortOrder::Desc;
  return fallback;
}

std::wstring JoinPath(const std::wstring& dir, const std::wstring& file) {
  if (dir.empty()) return file;
  if (dir.back() == L'\\') return dir + file;
  return dir + L'\\' + file;
}

std::wstring ReplaceAll(std::wstring input, const std::wstring& token, const std::wstring& value) {
  size_t pos = 0;
  while ((pos = input.find(token, pos)) != std::wstring::npos) {
    input.replace(pos, token.size(), value);
    pos += value.size();
  }
  return input;
}

std::wstring BuildCommand(const ActionConfig& action, const NoteGroupConfig& group, const NoteFile* file) {
  std::wstring command = action.command;
  command = ReplaceAll(command, L"{group}", group.id);
  command = ReplaceAll(command, L"{group_title}", group.title);
  command = ReplaceAll(command, L"{group_dir}", group.path);
  if (file) {
    command = ReplaceAll(command, L"{file}", file->path);
    command = ReplaceAll(command, L"{dir}", file->dir);
    command = ReplaceAll(command, L"{name}", file->name);
    command = ReplaceAll(command, L"{stem}", file->stem);
  } else {
    command = ReplaceAll(command, L"{file}", L"");
    command = ReplaceAll(command, L"{dir}", group.path);
    command = ReplaceAll(command, L"{name}", L"");
    command = ReplaceAll(command, L"{stem}", L"");
  }
  return command;
}

bool MatchesExtensionPattern(const std::wstring& pattern) {
  return pattern.size() >= 3 && pattern[0] == L'*' && pattern[1] == L'.';
}

std::wstring FormatWindowsErrorMessage(DWORD error) {
  wchar_t* buffer = nullptr;
  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageW(flags, nullptr, error, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  if (len == 0 || !buffer) {
    return L"Windows error " + std::to_wstring(error);
  }
  std::wstring message(buffer, len);
  LocalFree(buffer);
  while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' ' || message.back() == L'.')) {
    message.pop_back();
  }
  return message;
}
}

bool LoadNotesConfig(const std::wstring& path, NotesConfig& config) {
  if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
    return false;
  }

  config = NotesConfig{};
  config.defaultFilePatterns = SplitList(ReadString(path, L"notes", L"filePatterns", L"*.txt;*.md"));
  if (config.defaultFilePatterns.empty()) {
    config.defaultFilePatterns = {L"*.txt", L"*.md"};
  }
  config.defaultMaxItems = std::max(1, ReadInt(path, L"notes", L"maxItems", 5));
  config.defaultSortBy = ParseSortBy(ReadString(path, L"notes", L"sortBy", L"mtime"), NoteSortBy::ModifiedTime);
  config.defaultSortOrder = ParseSortOrder(ReadString(path, L"notes", L"sortOrder", L"desc"), SortOrder::Desc);
  config.defaultGroupExpanded = ReadBool(path, L"notes", L"defaultGroupExpanded", true);

  std::vector<wchar_t> sections(65536);
  DWORD len = GetPrivateProfileSectionNamesW(sections.data(), static_cast<DWORD>(sections.size()), path.c_str());
  if (len == 0) return true;

  const wchar_t* ptr = sections.data();
  while (*ptr) {
    std::wstring section = ptr;
    if (section.rfind(L"action.", 0) == 0) {
      ActionConfig action{};
      action.id = section.substr(7);
      action.title = ReadString(path, section.c_str(), L"title");
      action.command = ReadString(path, section.c_str(), L"command");
      if (!action.id.empty() && !action.title.empty() && !action.command.empty()) {
        config.actions[action.id] = action;
      }
    } else if (section.rfind(L"note_group.", 0) == 0) {
      NoteGroupConfig group{};
      group.id = section.substr(11);
      group.title = ReadString(path, section.c_str(), L"title");
      group.path = ReadString(path, section.c_str(), L"path");
      if (!group.id.empty() && !group.title.empty() && !group.path.empty()) {
        group.expanded = ReadBool(path, section.c_str(), L"expanded", config.defaultGroupExpanded);
        group.filePatterns = SplitList(ReadString(path, section.c_str(), L"filePatterns"));
        if (group.filePatterns.empty()) group.filePatterns = config.defaultFilePatterns;
        group.maxItems = std::max(1, ReadInt(path, section.c_str(), L"maxItems", config.defaultMaxItems));
        group.sortBy = ParseSortBy(ReadString(path, section.c_str(), L"sortBy"), config.defaultSortBy);
        group.sortOrder = ParseSortOrder(ReadString(path, section.c_str(), L"sortOrder"), config.defaultSortOrder);
        group.defaultFileAction = ReadString(path, section.c_str(), L"defaultFileAction");
        group.fileActions = SplitList(ReadString(path, section.c_str(), L"fileActions"));
        group.groupActions = SplitList(ReadString(path, section.c_str(), L"groupActions"));
        config.groups.push_back(group);
      }
    }
    ptr += section.size() + 1;
  }

  return true;
}

void LoadNoteFiles(const NoteGroupConfig& group, std::vector<NoteFile>& files, NoteGroupLoadState* state) {
  files.clear();
  if (state) *state = NoteGroupLoadState::Ok;
  std::set<std::wstring> seen;

  DWORD attr = GetFileAttributesW(group.path.c_str());
  if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    if (state) *state = NoteGroupLoadState::MissingDirectory;
    return;
  }

  for (const std::wstring& pattern : group.filePatterns) {
    WIN32_FIND_DATAW ffd{};
    HANDLE h = FindFirstFileW(JoinPath(group.path, pattern).c_str(), &ffd);
    if (h == INVALID_HANDLE_VALUE) continue;
    do {
      if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
      std::wstring fullPath = JoinPath(group.path, ffd.cFileName);
      if (!seen.insert(fullPath).second) continue;

      NoteFile file{};
      file.path = fullPath;
      file.dir = group.path;
      file.name = ffd.cFileName;
      file.stem = GetFileStem(file.name);
      file.createdTime = ffd.ftCreationTime;
      file.modifiedTime = ffd.ftLastWriteTime;
      files.push_back(file);
    } while (FindNextFileW(h, &ffd));
    FindClose(h);
  }

  auto compare = [&](const NoteFile& a, const NoteFile& b) {
    int order = 0;
    if (group.sortBy == NoteSortBy::Name) {
      order = _wcsicmp(a.name.c_str(), b.name.c_str());
      if (order == 0) {
        order = CompareFileTime(&b.modifiedTime, &a.modifiedTime);
      }
    } else if (group.sortBy == NoteSortBy::CreatedTime) {
      order = CompareFileTime(&a.createdTime, &b.createdTime);
      if (order == 0) {
        order = _wcsicmp(a.name.c_str(), b.name.c_str());
      }
    } else {
      order = CompareFileTime(&a.modifiedTime, &b.modifiedTime);
      if (order == 0) {
        order = _wcsicmp(a.name.c_str(), b.name.c_str());
      }
    }
    if (group.sortOrder == SortOrder::Desc) {
      return order > 0;
    }
    return order < 0;
  };

  std::sort(files.begin(), files.end(), compare);
  if (static_cast<int>(files.size()) > group.maxItems) {
    files.resize(group.maxItems);
  }
  if (files.empty() && state) {
    *state = NoteGroupLoadState::Empty;
  }
}

bool CreateNoteInGroup(const NoteGroupConfig& group, std::wstring& createdPath, std::wstring* errorMessage) {
  SYSTEMTIME st{};
  GetLocalTime(&st);

  DWORD attr = GetFileAttributesW(group.path.c_str());
  if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    if (errorMessage) *errorMessage = L"Group directory does not exist: " + group.path;
    return false;
  }

  std::wstring ext = L".txt";
  for (const std::wstring& pattern : group.filePatterns) {
    if (MatchesExtensionPattern(pattern)) {
      ext = pattern.substr(1);
      break;
    }
  }

  for (int attempt = 0; attempt < 100; ++attempt) {
    wchar_t name[128];
    if (attempt == 0) {
      wsprintfW(name, L"%04d%02d%02d-%02d%02d%02d%s",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, ext.c_str());
    } else {
      wsprintfW(name, L"%04d%02d%02d-%02d%02d%02d-%02d%s",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, attempt, ext.c_str());
    }
    createdPath = JoinPath(group.path, name);
    HANDLE h = CreateFileW(createdPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      CloseHandle(h);
      return true;
    }
    DWORD error = GetLastError();
    if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
      if (errorMessage) *errorMessage = L"Failed to create note file: " + FormatWindowsErrorMessage(error);
      return false;
    }
  }

  if (errorMessage) *errorMessage = L"Could not create a unique note filename in: " + group.path;
  return false;
}

bool ExecuteAction(const ActionConfig& action, const NoteGroupConfig& group, const NoteFile* file,
  std::wstring* errorMessage, std::wstring* resolvedCommand) {
  std::wstring command = BuildCommand(action, group, file);
  if (command.empty()) return false;
  if (resolvedCommand) *resolvedCommand = command;

  std::vector<wchar_t> buffer(command.begin(), command.end());
  buffer.push_back(L'\0');

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  BOOL ok = CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
  if (!ok) {
    if (errorMessage) *errorMessage = FormatWindowsErrorMessage(GetLastError());
    return false;
  }

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
}
