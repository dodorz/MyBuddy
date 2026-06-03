#include "notes.h"

#include "ini.h"

#include <algorithm>
#include <fstream>
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

std::wstring ReadString(const IniFile& ini, const wchar_t* section, const wchar_t* key, const wchar_t* def = L"") {
  return ini.GetString(section, key, def);
}

int ReadInt(const IniFile& ini, const wchar_t* section, const wchar_t* key, int def) {
  return ini.GetInt(section, key, def);
}

bool ReadBool(const IniFile& ini, const wchar_t* section, const wchar_t* key, bool def) {
  return ini.GetBool(section, key, def);
}

std::wstring GetFileStem(const std::wstring& name) {
  auto pos = name.find_last_of(L'.');
  if (pos == std::wstring::npos) return name;
  return name.substr(0, pos);
}

NoteSortBy ParseSortBy(const std::wstring& value, NoteSortBy fallback) {
  if (value == L"line") return NoteSortBy::LineOrder;
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

ActionTarget ParseActionTarget(const std::wstring& value, ActionTarget fallback) {
  if (value == L"file") return ActionTarget::File;
  if (value == L"dir" || value == L"directory" || value == L"group") return ActionTarget::Directory;
  return fallback;
}

NoteGroupType ParseGroupType(const std::wstring& value, NoteGroupType fallback) {
  if (value == L"dir" || value == L"directory") return NoteGroupType::Directory;
  if (value == L"text") return NoteGroupType::TextLines;
  if (value == L"todo" || value == L"todotxt") return NoteGroupType::TodoTxt;
  return fallback;
}

bool ParseGroupSection(const std::wstring& section, std::wstring& id, NoteGroupType& type) {
  if (section.rfind(L"dirgroup.", 0) == 0) {
    id = section.substr(9);
    type = NoteGroupType::Directory;
    return true;
  }
  if (section.rfind(L"textgroup.", 0) == 0) {
    id = section.substr(10);
    type = NoteGroupType::TextLines;
    return true;
  }
  if (section.rfind(L"todogroup.", 0) == 0) {
    id = section.substr(10);
    type = NoteGroupType::TodoTxt;
    return true;
  }
  if (section.rfind(L"note_group.", 0) == 0) {
    id = section.substr(11);
    return true;
  }
  return false;
}

bool IsSingleFileLineGroupType(NoteGroupType type) {
  return type == NoteGroupType::TextLines || type == NoteGroupType::TodoTxt;
}

NoteSortBy NormalizeSortByForGroupType(NoteGroupType type, NoteSortBy value, NoteSortBy fallback) {
  if (type == NoteGroupType::Directory && value == NoteSortBy::LineOrder) {
    return fallback;
  }
  return value;
}

std::wstring JoinPath(const std::wstring& dir, const std::wstring& file) {
  if (dir.empty()) return file;
  if (dir.back() == L'\\') return dir + file;
  return dir + L'\\' + file;
}

std::wstring GetParentDir(const std::wstring& path) {
  auto pos = path.find_last_of(L"\\/");
  if (pos == std::wstring::npos) return L"";
  return path.substr(0, pos);
}

std::wstring GetGroupDirectory(const NoteGroupConfig& group) {
  return group.type == NoteGroupType::Directory ? group.path : GetParentDir(group.path);
}

std::wstring EnsureTrailingSlash(std::wstring path) {
  if (!path.empty() && path.back() != L'\\') path.push_back(L'\\');
  return path;
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
  const std::wstring groupDir = GetGroupDirectory(group);
  command = ReplaceAll(command, L"{group}", group.id);
  command = ReplaceAll(command, L"{group_title}", group.title);
  command = ReplaceAll(command, L"{group_dir}", groupDir);
  if (file) {
    command = ReplaceAll(command, L"{file}", file->path);
    command = ReplaceAll(command, L"{dir}", file->dir);
    command = ReplaceAll(command, L"{name}", file->name);
    command = ReplaceAll(command, L"{stem}", file->stem);
    command = ReplaceAll(command, L"{line}", file->itemText);
    command = ReplaceAll(command, L"{line_number}", std::to_wstring(file->lineNumber));
  } else {
    command = ReplaceAll(command, L"{file}", L"");
    command = ReplaceAll(command, L"{dir}", groupDir);
    command = ReplaceAll(command, L"{name}", L"");
    command = ReplaceAll(command, L"{stem}", L"");
    command = ReplaceAll(command, L"{line}", L"");
    command = ReplaceAll(command, L"{line_number}", L"0");
  }
  return command;
}

bool MatchesExtensionPattern(const std::wstring& pattern) {
  return pattern.size() >= 3 && pattern[0] == L'*' && pattern[1] == L'.';
}

std::wstring NormalizeExtension(std::wstring value) {
  if (value.empty()) return value;
  if (value[0] != L'.') value.insert(value.begin(), L'.');
  return value;
}

int NormalizeMaxItems(int value, int fallback) {
  if (value < 0) return fallback;
  return value;
}

std::wstring FormatWindowsErrorMessage(DWORD error);

std::wstring ToLower(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(), towlower);
  return value;
}

std::wstring Trim(std::wstring value) {
  size_t start = 0;
  while (start < value.size() && iswspace(value[start])) ++start;
  size_t end = value.size();
  while (end > start && iswspace(value[end - 1])) --end;
  return value.substr(start, end - start);
}

std::wstring TrimRight(std::wstring value) {
  size_t end = value.size();
  while (end > 0 && iswspace(value[end - 1])) --end;
  value.resize(end);
  return value;
}

std::wstring StripMdHeading(std::wstring value) {
  value = Trim(value);
  size_t i = 0;
  while (i < value.size() && value[i] == L'#') ++i;
  if (i > 0 && i < value.size() && iswspace(value[i])) {
    while (i < value.size() && iswspace(value[i])) ++i;
    value = value.substr(i);
  }
  return Trim(value);
}

std::wstring StripMarkdownLinePrefix(std::wstring value) {
  value = StripMdHeading(value);
  value = Trim(value);

  auto stripPrefixToken = [&](const std::wstring& token) {
    if (value.rfind(token, 0) == 0) {
      value = Trim(value.substr(token.size()));
      return true;
    }
    return false;
  };

  if (stripPrefixToken(L"- ") || stripPrefixToken(L"+ ") || stripPrefixToken(L"* ")) {
    return value;
  }
  if (stripPrefixToken(L"** ") || stripPrefixToken(L"*** ")) {
    return value;
  }

  size_t index = 0;
  while (index < value.size() && iswdigit(value[index])) ++index;
  if (index > 0 && index + 1 < value.size() &&
      value[index] == L'.' && iswspace(value[index + 1])) {
    value = Trim(value.substr(index + 1));
    return value;
  }

  return value;
}

bool IsIsoDateToken(const std::wstring& token) {
  if (token.size() != 10) return false;
  for (size_t i = 0; i < token.size(); ++i) {
    if (i == 4 || i == 7) {
      if (token[i] != L'-') return false;
      continue;
    }
    if (!iswdigit(token[i])) return false;
  }
  return true;
}

std::wstring ConsumeTodoTxtDateToken(std::wstring text) {
  text = Trim(text);
  const size_t space = text.find_first_of(L" \t");
  const std::wstring token = space == std::wstring::npos ? text : text.substr(0, space);
  if (!IsIsoDateToken(token)) return text;
  return space == std::wstring::npos ? L"" : Trim(text.substr(space + 1));
}

std::wstring RenderTodoTxtTaskDisplay(const std::wstring& rawLine) {
  const std::wstring trimmed = TrimRight(rawLine);
  std::wstring text = Trim(trimmed);
  if (text.empty()) return L"";

  bool completed = false;
  if (text.size() >= 2 && text[0] == L'x' && iswspace(text[1])) {
    completed = true;
    text = Trim(text.substr(2));
    text = ConsumeTodoTxtDateToken(text);
    text = ConsumeTodoTxtDateToken(text);
  }

  if (text.empty()) text = Trim(trimmed);
  return completed ? (L"- [x] " + text) : (L"- [ ] " + text);
}

std::wstring SanitizeFileBaseName(std::wstring value) {
  static const std::wstring invalid = L"<>:\"/\\|?*";
  value = Trim(value);
  for (wchar_t& ch : value) {
    if (ch < 32 || invalid.find(ch) != std::wstring::npos) ch = L'_';
  }
  while (!value.empty() && (value.back() == L' ' || value.back() == L'.')) value.pop_back();
  if (value.empty()) value = L"note";
  if (value.size() > 64) value.resize(64);
  value = Trim(value);
  if (value.empty()) value = L"note";
  return value;
}

std::wstring DecodeTextBytes(const std::string& bytes) {
  if (bytes.empty()) return L"";
  const char* data = bytes.data();
  int size = static_cast<int>(bytes.size());
  if (size >= 3 && static_cast<unsigned char>(data[0]) == 0xEF &&
      static_cast<unsigned char>(data[1]) == 0xBB &&
      static_cast<unsigned char>(data[2]) == 0xBF) {
    data += 3;
    size -= 3;
  }
  int wideLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, size, nullptr, 0);
  if (wideLen > 0) {
    std::wstring text(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, size, text.data(), wideLen);
    return text;
  }
  wideLen = MultiByteToWideChar(CP_ACP, 0, data, size, nullptr, 0);
  if (wideLen > 0) {
    std::wstring text(wideLen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, data, size, text.data(), wideLen);
    return text;
  }
  return L"";
}

std::wstring ReadTextFilePrefix(const std::wstring& path, size_t maxBytes = 0) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return L"";
  std::string bytes;
  if (maxBytes == 0) {
    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  } else {
    bytes.resize(maxBytes);
    in.read(bytes.data(), static_cast<std::streamsize>(maxBytes));
    bytes.resize(static_cast<size_t>(in.gcount()));
  }
  return DecodeTextBytes(bytes);
}

std::wstring ReadTextFile(const std::wstring& path) {
  return ReadTextFilePrefix(path, 0);
}

std::vector<std::wstring> SplitLines(const std::wstring& text) {
  std::vector<std::wstring> lines;
  std::wstring current;
  for (wchar_t ch : text) {
    if (ch == L'\r') continue;
    if (ch == L'\n') {
      lines.push_back(current);
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  lines.push_back(current);
  return lines;
}

bool GroupSupportsNewNotes(const NoteGroupConfig& group) {
  return group.type == NoteGroupType::Directory;
}

enum class TextFileEncoding {
  Utf8NoBom,
  Utf8Bom,
  Acp,
};

struct EditableTextFile {
  std::wstring text;
  std::wstring newline = L"\r\n";
  TextFileEncoding encoding = TextFileEncoding::Utf8NoBom;
};

bool ReadBinaryFile(const std::wstring& path, std::string& bytes, std::wstring* errorMessage) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (errorMessage) *errorMessage = L"Failed to open file: " + FormatWindowsErrorMessage(GetLastError());
    return false;
  }
  bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

bool DecodeBytesWithCodePage(const char* data, int size, UINT codePage, DWORD flags, std::wstring& text) {
  int wideLen = MultiByteToWideChar(codePage, flags, data, size, nullptr, 0);
  if (wideLen <= 0) return false;
  text.assign(static_cast<size_t>(wideLen), L'\0');
  return MultiByteToWideChar(codePage, flags, data, size, text.data(), wideLen) > 0;
}

bool ReadEditableTextFile(const std::wstring& path, EditableTextFile& file, std::wstring* errorMessage) {
  std::string bytes;
  if (!ReadBinaryFile(path, bytes, errorMessage)) return false;

  const char* data = bytes.data();
  int size = static_cast<int>(bytes.size());
  bool utf8Bom = false;
  if (size >= 3 &&
      static_cast<unsigned char>(data[0]) == 0xEF &&
      static_cast<unsigned char>(data[1]) == 0xBB &&
      static_cast<unsigned char>(data[2]) == 0xBF) {
    utf8Bom = true;
    data += 3;
    size -= 3;
  }

  file = EditableTextFile{};
  if (utf8Bom) file.encoding = TextFileEncoding::Utf8Bom;

  if (!DecodeBytesWithCodePage(data, size, CP_UTF8, MB_ERR_INVALID_CHARS, file.text)) {
    if (!DecodeBytesWithCodePage(data, size, CP_ACP, 0, file.text)) {
      if (errorMessage) *errorMessage = L"Failed to decode text file.";
      return false;
    }
    file.encoding = TextFileEncoding::Acp;
  } else if (!utf8Bom) {
    file.encoding = TextFileEncoding::Utf8NoBom;
  }

  if (file.text.find(L"\r\n") != std::wstring::npos) {
    file.newline = L"\r\n";
  } else if (file.text.find(L'\n') != std::wstring::npos) {
    file.newline = L"\n";
  } else if (file.text.find(L'\r') != std::wstring::npos) {
    file.newline = L"\r";
  }
  return true;
}

bool EncodeTextWithCodePage(const std::wstring& text, UINT codePage, std::string& bytes) {
  int byteLen = WideCharToMultiByte(codePage, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (byteLen < 0) return false;
  bytes.assign(static_cast<size_t>(byteLen), '\0');
  return WideCharToMultiByte(codePage, 0, text.data(), static_cast<int>(text.size()), bytes.data(), byteLen, nullptr, nullptr) > 0;
}

bool WriteEditableTextFile(const std::wstring& path, const EditableTextFile& file, const std::vector<std::wstring>& lines,
  std::wstring* errorMessage) {
  std::wstring text;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) text += file.newline;
    text += lines[i];
  }

  std::string bytes;
  UINT codePage = file.encoding == TextFileEncoding::Acp ? CP_ACP : CP_UTF8;
  if (!EncodeTextWithCodePage(text, codePage, bytes)) {
    if (errorMessage) *errorMessage = L"Failed to encode updated text file.";
    return false;
  }
  if (file.encoding == TextFileEncoding::Utf8Bom) {
    bytes.insert(bytes.begin(), {'\xEF', '\xBB', '\xBF'});
  }

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    if (errorMessage) *errorMessage = L"Failed to open file for writing: " + FormatWindowsErrorMessage(GetLastError());
    return false;
  }
  out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!out.good()) {
    if (errorMessage) *errorMessage = L"Failed to write updated file.";
    return false;
  }
  return true;
}

bool ToggleMarkdownCheckboxLine(std::wstring& line) {
  size_t start = 0;
  while (start < line.size() && (line[start] == L' ' || line[start] == L'\t')) ++start;
  if (start + 5 > line.size()) return false;
  if (line[start] != L'-' || line[start + 1] != L' ' || line[start + 2] != L'[' || line[start + 4] != L']') {
    return false;
  }
  if (line[start + 3] == L' ') {
    line[start + 3] = L'x';
    return true;
  }
  if (line[start + 3] == L'x' || line[start + 3] == L'X') {
    line[start + 3] = L' ';
    return true;
  }
  return false;
}

bool TryFindTomlFrontMatter(const std::vector<std::wstring>& lines, size_t& contentStartLine) {
  contentStartLine = 0;
  if (lines.empty()) return false;
  const std::wstring delimiter = Trim(lines[0]);
  if (delimiter != L"+++" && delimiter != L"---") return false;
  for (size_t i = 1; i < lines.size(); ++i) {
    if (Trim(lines[i]) == delimiter) {
      contentStartLine = i + 1;
      return true;
    }
  }
  return false;
}

std::wstring TryParseTomlTitle(const std::vector<std::wstring>& lines, size_t& contentStartLine) {
  if (!TryFindTomlFrontMatter(lines, contentStartLine)) return L"";
  const std::wstring delimiter = Trim(lines[0]);
  for (size_t i = 1; i < lines.size(); ++i) {
    std::wstring line = Trim(lines[i]);
    if (line == delimiter) {
      contentStartLine = i + 1;
      break;
    }
    size_t separator = line.find(L'=');
    size_t valueStart = 0;
    if (separator != std::wstring::npos) {
      valueStart = separator + 1;
    } else {
      separator = line.find(L':');
      if (separator == std::wstring::npos) continue;
      valueStart = separator + 1;
    }
    std::wstring key = Trim(line.substr(0, separator));
    if (ToLower(key) != L"title") continue;
    std::wstring value = Trim(line.substr(valueStart));
    if (value.size() >= 2 &&
        ((value.front() == L'"' && value.back() == L'"') ||
         (value.front() == L'\'' && value.back() == L'\''))) {
      value = value.substr(1, value.size() - 2);
    }
    return Trim(value);
  }
  return L"";
}

std::wstring ExtractNoteBaseName(const std::wstring& path) {
  std::wstring text = ReadTextFilePrefix(path, 64 * 1024);
  std::vector<std::wstring> lines = SplitLines(text);
  std::wstring extension = ToLower(NormalizeExtension(path.substr(path.find_last_of(L'.'))));

  std::wstring candidate;
  if (extension == L".md") {
    size_t contentStart = 0;
    candidate = TryParseTomlTitle(lines, contentStart);
    if (candidate.empty()) {
      for (size_t i = contentStart; i < lines.size(); ++i) {
        candidate = StripMarkdownLinePrefix(lines[i]);
        if (!candidate.empty()) break;
      }
    }
  } else {
    if (!lines.empty()) candidate = Trim(lines[0]);
  }

  return SanitizeFileBaseName(candidate);
}

std::wstring ExtractTextGroupTitle(const std::wstring& path) {
  const size_t slash = path.find_last_of(L"\\/");
  const std::wstring sourceName = slash == std::wstring::npos ? path : path.substr(slash + 1);
  const std::wstring sourceStem = GetFileStem(sourceName);
  const size_t dot = path.find_last_of(L'.');
  const std::wstring extension = dot == std::wstring::npos
    ? L""
    : ToLower(NormalizeExtension(path.substr(dot)));
  if (extension != L".md") {
    return sourceStem.empty() ? sourceName : sourceStem;
  }

  std::vector<std::wstring> lines = SplitLines(ReadTextFilePrefix(path, 64 * 1024));
  size_t contentStart = 0;
  std::wstring title = TryParseTomlTitle(lines, contentStart);
  if (!title.empty()) return title;

  size_t firstContentLine = contentStart;
  while (firstContentLine < lines.size() && Trim(lines[firstContentLine]).empty()) ++firstContentLine;

  for (size_t i = contentStart; i < lines.size(); ++i) {
    std::wstring line = Trim(lines[i]);
    if (line.rfind(L"# ", 0) == 0) {
      title = StripMdHeading(line);
      if (!title.empty()) return title;
    }
  }

  if (firstContentLine < lines.size()) {
    std::wstring firstLine = Trim(lines[firstContentLine]);
    if (firstLine.rfind(L"#", 0) == 0) {
      title = StripMdHeading(firstLine);
      if (!title.empty()) return title;
    }
  }

  return sourceStem.empty() ? sourceName : sourceStem;
}

std::wstring ExtractSingleFileGroupTitle(const std::wstring& path, NoteGroupType type) {
  if (type == NoteGroupType::TextLines) return ExtractTextGroupTitle(path);
  const size_t slash = path.find_last_of(L"\\/");
  const std::wstring sourceName = slash == std::wstring::npos ? path : path.substr(slash + 1);
  const std::wstring sourceStem = GetFileStem(sourceName);
  return sourceStem.empty() ? sourceName : sourceStem;
}

std::wstring ExtractMarkdownDisplayName(const std::wstring& path, const std::wstring& fallback) {
  std::vector<std::wstring> lines = SplitLines(ReadTextFilePrefix(path, 64 * 1024));
  size_t contentStart = 0;
  std::wstring title = TryParseTomlTitle(lines, contentStart);
  if (!title.empty()) return title;

  size_t firstContentLine = contentStart;
  while (firstContentLine < lines.size() && Trim(lines[firstContentLine]).empty()) ++firstContentLine;

  for (size_t i = contentStart; i < lines.size(); ++i) {
    std::wstring line = Trim(lines[i]);
    if (line.rfind(L"# ", 0) == 0) {
      title = StripMdHeading(line);
      if (!title.empty()) return title;
    }
  }

  if (firstContentLine < lines.size()) {
    std::wstring firstLine = Trim(lines[firstContentLine]);
    if (firstLine.rfind(L"#", 0) == 0) {
      title = StripMdHeading(firstLine);
      if (!title.empty()) return title;
    }
  }

  return fallback;
}

std::wstring ChooseCreateExtension(const NoteGroupConfig& group) {
  if (!group.createExtension.empty()) return group.createExtension;
  for (const std::wstring& pattern : group.filePatterns) {
    if (MatchesExtensionPattern(pattern)) return NormalizeExtension(pattern.substr(1));
  }
  return L".txt";
}

bool CreateEmptyFile(const std::wstring& path, std::wstring* errorMessage) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    if (errorMessage) *errorMessage = L"Failed to create note file: " + FormatWindowsErrorMessage(GetLastError());
    return false;
  }
  CloseHandle(h);
  return true;
}

bool BuildUniqueTargetPath(const std::wstring& dir, const std::wstring& baseName, const std::wstring& extension,
  std::wstring& path, std::wstring* errorMessage) {
  for (int attempt = 0; attempt < 100; ++attempt) {
    std::wstring fileName = baseName;
    if (attempt > 0) fileName += L"-" + std::to_wstring(attempt + 1);
    fileName += extension;
    path = JoinPath(dir, fileName);
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND) return true;
  }
  if (errorMessage) *errorMessage = L"Could not allocate a unique target filename in: " + dir;
  return false;
}

bool SplitExecutableAndParameters(const std::wstring& command, std::wstring& executable, std::wstring& parameters) {
  executable.clear();
  parameters.clear();

  const size_t first = command.find_first_not_of(L" \t\r\n");
  if (first == std::wstring::npos) return false;

  size_t cursor = first;
  if (command[cursor] == L'"') {
    ++cursor;
    const size_t endQuote = command.find(L'"', cursor);
    if (endQuote == std::wstring::npos) return false;
    executable = command.substr(cursor, endQuote - cursor);
    cursor = endQuote + 1;
  } else {
    const size_t end = command.find_first_of(L" \t\r\n", cursor);
    if (end == std::wstring::npos) {
      executable = command.substr(cursor);
      return !executable.empty();
    }
    executable = command.substr(cursor, end - cursor);
    cursor = end;
  }

  parameters = Trim(command.substr(cursor));
  return !executable.empty();
}

bool StartActionProcess(const ActionConfig& action, const NoteGroupConfig& group, const NoteFile* file,
  HANDLE& processHandle, std::wstring* errorMessage, std::wstring* resolvedCommand) {
  if (!IsActionTargetCompatible(action, file)) {
    if (errorMessage) {
      *errorMessage = action.target == ActionTarget::Directory
        ? L"Directory action cannot be run for a file item."
        : L"File action requires a file item.";
    }
    return false;
  }

  std::wstring command = BuildCommand(action, group, file);
  if (command.empty()) return false;
  if (resolvedCommand) *resolvedCommand = command;

  std::wstring executable;
  std::wstring parameters;
  if (!SplitExecutableAndParameters(command, executable, parameters)) {
    if (errorMessage) *errorMessage = L"Invalid command line.";
    return false;
  }

  const std::wstring workingDirectory = file ? file->dir : GetGroupDirectory(group);
  SHELLEXECUTEINFOW sei{};
  sei.cbSize = sizeof(sei);
  sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
  sei.lpVerb = L"open";
  sei.lpFile = executable.c_str();
  sei.lpParameters = parameters.empty() ? nullptr : parameters.c_str();
  sei.lpDirectory = workingDirectory.empty() ? nullptr : workingDirectory.c_str();
  sei.nShow = SW_SHOWNORMAL;

  BOOL ok = ShellExecuteExW(&sei);
  if (!ok) {
    if (errorMessage) *errorMessage = FormatWindowsErrorMessage(GetLastError());
    return false;
  }
  processHandle = sei.hProcess;
  return true;
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

  IniFile ini;
  if (!ini.LoadUtf8(path)) {
    return false;
  }

  config = NotesConfig{};
  config.defaultGroupExpanded = ReadBool(ini, L"notes_default", L"defaultGroupExpanded", true);

  config.sharedDefaults.filePatterns = SplitList(ReadString(ini, L"notes_default", L"filePatterns", L"*.txt;*.md"));
  if (config.sharedDefaults.filePatterns.empty()) {
    config.sharedDefaults.filePatterns = {L"*.txt", L"*.md"};
  }
  config.sharedDefaults.createExtension = NormalizeExtension(ReadString(ini, L"notes_default", L"createExtension"));
  config.sharedDefaults.maxItems = NormalizeMaxItems(ReadInt(ini, L"notes_default", L"maxItems", 5), 5);
  config.sharedDefaults.showExtensions = false;
  config.sharedDefaults.defaultFileAction = ReadString(ini, L"notes_default", L"defaultFileAction");
  config.sharedDefaults.deleteCommand = ReadString(ini, L"notes_default", L"deleteCommand");
  config.sharedDefaults.fileActions = SplitList(ReadString(ini, L"notes_default", L"fileActions"));
  config.sharedDefaults.groupActions = SplitList(ReadString(ini, L"notes_default", L"groupActions"));

  config.dirDefaults = config.sharedDefaults;
  config.dirDefaults.sortBy = NormalizeSortByForGroupType(NoteGroupType::Directory,
    ParseSortBy(ReadString(ini, L"notes_default", L"sortBy", L"mtime"), NoteSortBy::ModifiedTime),
    NoteSortBy::ModifiedTime);
  config.dirDefaults.sortOrder = ParseSortOrder(ReadString(ini, L"notes_default", L"sortOrder", L"desc"), SortOrder::Desc);
  config.dirDefaults.filePatterns = SplitList(ReadString(ini, L"notes_dir_default", L"filePatterns"));
  if (config.dirDefaults.filePatterns.empty()) {
    config.dirDefaults.filePatterns = config.sharedDefaults.filePatterns;
  }
  config.dirDefaults.createExtension = NormalizeExtension(ReadString(ini, L"notes_dir_default", L"createExtension",
    config.dirDefaults.createExtension.c_str()));
  config.dirDefaults.maxItems = NormalizeMaxItems(
    ReadInt(ini, L"notes_dir_default", L"maxItems", config.dirDefaults.maxItems), config.dirDefaults.maxItems);
  config.dirDefaults.sortBy = NormalizeSortByForGroupType(NoteGroupType::Directory,
    ParseSortBy(ReadString(ini, L"notes_dir_default", L"sortBy"), config.dirDefaults.sortBy),
    config.dirDefaults.sortBy);
  config.dirDefaults.sortOrder = ParseSortOrder(ReadString(ini, L"notes_dir_default", L"sortOrder"), config.dirDefaults.sortOrder);
  config.dirDefaults.showExtensions = ReadBool(ini, L"notes_dir_default", L"showExtensions", config.dirDefaults.showExtensions);
  config.dirDefaults.defaultFileAction = ReadString(ini, L"notes_dir_default", L"defaultFileAction",
    config.dirDefaults.defaultFileAction.c_str());
  config.dirDefaults.deleteCommand = ReadString(ini, L"notes_dir_default", L"deleteCommand",
    config.dirDefaults.deleteCommand.c_str());
  {
    std::vector<std::wstring> actions = SplitList(ReadString(ini, L"notes_dir_default", L"fileActions"));
    if (!actions.empty()) config.dirDefaults.fileActions = std::move(actions);
  }
  {
    std::vector<std::wstring> actions = SplitList(ReadString(ini, L"notes_dir_default", L"groupActions"));
    if (!actions.empty()) config.dirDefaults.groupActions = std::move(actions);
  }

  config.textDefaults = config.sharedDefaults;
  config.textDefaults.sortBy = NoteSortBy::LineOrder;
  config.textDefaults.sortOrder = SortOrder::Asc;
  config.textDefaults.filePatterns.clear();
  config.textDefaults.createExtension = NormalizeExtension(ReadString(ini, L"notes_text_default", L"createExtension",
    config.textDefaults.createExtension.c_str()));
  config.textDefaults.maxItems = NormalizeMaxItems(
    ReadInt(ini, L"notes_text_default", L"maxItems", config.textDefaults.maxItems), config.textDefaults.maxItems);
  config.textDefaults.sortBy = ParseSortBy(ReadString(ini, L"notes_text_default", L"sortBy"), config.textDefaults.sortBy);
  config.textDefaults.sortOrder = ParseSortOrder(ReadString(ini, L"notes_text_default", L"sortOrder"), config.textDefaults.sortOrder);
  config.textDefaults.showExtensions = false;
  config.textDefaults.defaultFileAction = ReadString(ini, L"notes_text_default", L"defaultFileAction",
    config.textDefaults.defaultFileAction.c_str());
  config.textDefaults.deleteCommand = ReadString(ini, L"notes_text_default", L"deleteCommand",
    config.textDefaults.deleteCommand.c_str());
  {
    std::vector<std::wstring> actions = SplitList(ReadString(ini, L"notes_text_default", L"fileActions"));
    if (!actions.empty()) config.textDefaults.fileActions = std::move(actions);
  }
  {
    std::vector<std::wstring> actions = SplitList(ReadString(ini, L"notes_text_default", L"groupActions"));
    if (!actions.empty()) config.textDefaults.groupActions = std::move(actions);
  }

  std::vector<std::wstring> sections = ini.GetSectionNames();
  for (const std::wstring& section : sections) {
    if (section.rfind(L"file_action.", 0) == 0 || section.rfind(L"dir_action.", 0) == 0) {
      ActionConfig action{};
      if (section.rfind(L"file_action.", 0) == 0) {
        action.id = section.substr(12);
        action.target = ActionTarget::File;
      } else {
        action.id = section.substr(11);
        action.target = ActionTarget::Directory;
      }
      action.title = ReadString(ini, section.c_str(), L"title");
      if (action.title.empty()) action.title = action.id;
      action.command = ReadString(ini, section.c_str(), L"command");
      if (!action.id.empty() && !action.title.empty() && !action.command.empty()) {
        config.actions[action.id] = action;
      }
    } else {
      NoteGroupConfig group{};
      if (!ParseGroupSection(section, group.id, group.type)) {
        continue;
      }
      group.title = ReadString(ini, section.c_str(), L"title");
      group.path = ReadString(ini, section.c_str(), L"path");
      if (section.rfind(L"note_group.", 0) == 0) {
        group.type = ParseGroupType(ReadString(ini, section.c_str(), L"type", L"dir"), group.type);
      }
      if (group.title.empty()) {
        if (IsSingleFileLineGroupType(group.type) && !group.path.empty()) {
          group.title = ExtractSingleFileGroupTitle(group.path, group.type);
        }
        if (group.title.empty()) group.title = group.id;
      }
      if (!group.id.empty() && !group.title.empty() && !group.path.empty()) {
        const NoteGroupDefaults& defaults =
          group.type == NoteGroupType::Directory ? config.dirDefaults : config.textDefaults;
        group.expanded = ReadBool(ini, section.c_str(), L"expanded", config.defaultGroupExpanded);
        group.showExtensions = group.type == NoteGroupType::Directory
          ? ReadBool(ini, section.c_str(), L"showExtensions", defaults.showExtensions)
          : false;
        group.filePatterns = SplitList(ReadString(ini, section.c_str(), L"filePatterns"));
        if (group.filePatterns.empty()) group.filePatterns = defaults.filePatterns;
        group.createExtension = NormalizeExtension(ReadString(ini, section.c_str(), L"createExtension", defaults.createExtension.c_str()));
        group.maxItems = NormalizeMaxItems(ReadInt(ini, section.c_str(), L"maxItems", defaults.maxItems), defaults.maxItems);
        std::wstring rawSortBy = ReadString(ini, section.c_str(), L"sortBy");
        group.sortBy = NormalizeSortByForGroupType(group.type, ParseSortBy(rawSortBy, defaults.sortBy), defaults.sortBy);
        group.sortOrder = ParseSortOrder(ReadString(ini, section.c_str(), L"sortOrder"), defaults.sortOrder);
        group.defaultFileAction = ReadString(ini, section.c_str(), L"defaultFileAction", defaults.defaultFileAction.c_str());
        group.deleteCommand = ReadString(ini, section.c_str(), L"deleteCommand", defaults.deleteCommand.c_str());
        group.fileActions = SplitList(ReadString(ini, section.c_str(), L"fileActions"));
        if (group.fileActions.empty()) group.fileActions = defaults.fileActions;
        group.groupActions = SplitList(ReadString(ini, section.c_str(), L"groupActions"));
        if (group.groupActions.empty()) group.groupActions = defaults.groupActions;
        config.groups.push_back(group);
      }
    }
  }

  return true;
}

void LoadNoteFiles(const NoteGroupConfig& group, std::vector<NoteFile>& files, NoteGroupLoadState* state, bool ignoreMaxItems) {
  files.clear();
  if (state) *state = NoteGroupLoadState::Ok;
  std::set<std::wstring> seen;

  DWORD attr = GetFileAttributesW(group.path.c_str());
  if (group.type == NoteGroupType::Directory) {
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      if (state) *state = NoteGroupLoadState::MissingPath;
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
        file.displayName.clear();
        file.itemText = file.stem;
        file.createdTime = ffd.ftCreationTime;
        file.modifiedTime = ffd.ftLastWriteTime;
        files.push_back(file);
      } while (FindNextFileW(h, &ffd));
      FindClose(h);
    }
  } else {
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      if (state) *state = NoteGroupLoadState::MissingPath;
      return;
    }

    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(group.path.c_str(), GetFileExInfoStandard, &data)) {
      if (state) *state = NoteGroupLoadState::MissingPath;
      return;
    }

    std::vector<std::wstring> lines = SplitLines(ReadTextFile(group.path));
    const std::wstring sourceName = group.path.substr(group.path.find_last_of(L"\\/") + 1);
    const std::wstring sourceStem = GetFileStem(sourceName);
    const std::wstring sourceDir = GetParentDir(group.path);
    if (group.type == NoteGroupType::TodoTxt) {
      for (size_t i = 0; i < lines.size(); ++i) {
        std::wstring rawText = TrimRight(lines[i]);
        if (Trim(rawText).empty()) continue;

        NoteFile file{};
        file.path = group.path;
        file.dir = sourceDir;
        file.name = sourceName;
        file.stem = sourceStem;
        file.displayName = RenderTodoTxtTaskDisplay(rawText);
        file.itemText = rawText;
        file.lineNumber = static_cast<int>(i + 1);
        file.createdTime = data.ftCreationTime;
        file.modifiedTime = data.ftLastWriteTime;
        files.push_back(file);
      }
    } else {
      size_t contentStart = 0;
      const std::wstring sourceExtension = ToLower(NormalizeExtension(group.path.substr(group.path.find_last_of(L'.'))));
      if (sourceExtension == L".md") {
        TryFindTomlFrontMatter(lines, contentStart);
        size_t firstContentLine = contentStart;
        while (firstContentLine < lines.size() && Trim(lines[firstContentLine]).empty()) ++firstContentLine;
        if (firstContentLine < lines.size() && Trim(lines[firstContentLine]).rfind(L"# ", 0) == 0) {
          contentStart = firstContentLine + 1;
        }
      }
      for (size_t i = contentStart; i < lines.size(); ++i) {
        if (Trim(lines[i]).empty()) continue;
        std::wstring text = TrimRight(lines[i]);

        NoteFile file{};
        file.path = group.path;
        file.dir = sourceDir;
        file.name = sourceName;
        file.stem = sourceStem;
        file.displayName = text;
        file.itemText = text;
        file.lineNumber = static_cast<int>(i + 1);
        file.createdTime = data.ftCreationTime;
        file.modifiedTime = data.ftLastWriteTime;
        files.push_back(file);
      }
    }
  }

  auto compare = [&](const NoteFile& a, const NoteFile& b) {
    int order = 0;
    if (group.sortBy == NoteSortBy::LineOrder) {
      order = a.lineNumber - b.lineNumber;
    } else if (group.sortBy == NoteSortBy::Name) {
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
  if (!ignoreMaxItems && group.maxItems > 0 && static_cast<int>(files.size()) > group.maxItems) {
    files.resize(group.maxItems);
  }
  if (group.type == NoteGroupType::Directory) {
    for (NoteFile& file : files) {
      const size_t fileDot = file.name.find_last_of(L'.');
      const std::wstring fileExtension = fileDot == std::wstring::npos
        ? L""
        : ToLower(NormalizeExtension(file.name.substr(fileDot)));
      if (fileExtension == L".md") {
        file.displayName = ExtractMarkdownDisplayName(file.path, file.stem);
        file.itemText = StripMarkdownLinePrefix(file.displayName);
        if (file.itemText.empty()) file.itemText = file.stem;
      } else {
        file.displayName.clear();
        file.itemText = file.stem;
      }
    }
  }
  if (files.empty() && state) {
    *state = NoteGroupLoadState::Empty;
  }
}

bool ToggleMarkdownCheckbox(const NoteFile& file, std::wstring* errorMessage) {
  if (file.path.empty() || file.lineNumber <= 0) {
    if (errorMessage) *errorMessage = L"Invalid note item.";
    return false;
  }

  EditableTextFile textFile{};
  if (!ReadEditableTextFile(file.path, textFile, errorMessage)) return false;

  std::vector<std::wstring> lines = SplitLines(textFile.text);
  const size_t index = static_cast<size_t>(file.lineNumber - 1);
  if (index >= lines.size()) {
    if (errorMessage) *errorMessage = L"Target line is out of range.";
    return false;
  }
  if (!ToggleMarkdownCheckboxLine(lines[index])) {
    if (errorMessage) *errorMessage = L"Target line is not a markdown checkbox.";
    return false;
  }

  return WriteEditableTextFile(file.path, textFile, lines, errorMessage);
}

bool ToggleTodoTxtTask(const NoteFile& file, std::wstring* errorMessage) {
  if (file.path.empty() || file.lineNumber <= 0) {
    if (errorMessage) *errorMessage = L"Invalid task item.";
    return false;
  }

  EditableTextFile textFile{};
  if (!ReadEditableTextFile(file.path, textFile, errorMessage)) return false;

  std::vector<std::wstring> lines = SplitLines(textFile.text);
  const size_t index = static_cast<size_t>(file.lineNumber - 1);
  if (index >= lines.size()) {
    if (errorMessage) *errorMessage = L"Target line is out of range.";
    return false;
  }

  std::wstring line = TrimRight(lines[index]);
  size_t indentEnd = 0;
  while (indentEnd < line.size() && (line[indentEnd] == L' ' || line[indentEnd] == L'\t')) ++indentEnd;
  const std::wstring indent = line.substr(0, indentEnd);
  std::wstring body = Trim(line.substr(indentEnd));
  if (body.empty()) {
    if (errorMessage) *errorMessage = L"Target line is empty.";
    return false;
  }

  if (body.size() >= 2 && body[0] == L'x' && iswspace(body[1])) {
    body = Trim(body.substr(2));
    body = ConsumeTodoTxtDateToken(body);
    lines[index] = indent + body;
  } else {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t date[16];
    wsprintfW(date, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    lines[index] = indent + L"x " + date + L" " + body;
  }

  return WriteEditableTextFile(file.path, textFile, lines, errorMessage);
}

bool CreateNoteInGroup(const NoteGroupConfig& group, std::wstring& createdPath, std::wstring* errorMessage) {
  if (group.type != NoteGroupType::Directory) {
    if (errorMessage) *errorMessage = L"Only directory groups support creating new notes.";
    return false;
  }
  SYSTEMTIME st{};
  GetLocalTime(&st);

  DWORD attr = GetFileAttributesW(group.path.c_str());
  if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    if (errorMessage) *errorMessage = L"Group directory does not exist: " + group.path;
    return false;
  }

  std::wstring ext = L".txt";
  if (!group.createExtension.empty()) {
    ext = group.createExtension;
  } else {
    for (const std::wstring& pattern : group.filePatterns) {
      if (MatchesExtensionPattern(pattern)) {
        ext = pattern.substr(1);
        break;
      }
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

bool CreateTempNoteForGroup(const NoteGroupConfig& group, std::wstring& createdPath, std::wstring* errorMessage) {
  if (group.type != NoteGroupType::Directory) {
    if (errorMessage) *errorMessage = L"Only directory groups support creating new notes.";
    return false;
  }
  wchar_t tempPath[MAX_PATH];
  DWORD len = GetTempPathW(MAX_PATH, tempPath);
  if (len == 0 || len >= MAX_PATH) {
    if (errorMessage) *errorMessage = L"Failed to locate temporary directory.";
    return false;
  }

  std::wstring tempDir = EnsureTrailingSlash(tempPath);
  tempDir += L"MyBuddy\\Drafts";
  CreateDirectoryW(JoinPath(EnsureTrailingSlash(tempPath), L"MyBuddy").c_str(), nullptr);
  CreateDirectoryW(tempDir.c_str(), nullptr);

  std::wstring ext = ChooseCreateExtension(group);
  for (int attempt = 0; attempt < 100; ++attempt) {
    wchar_t name[160];
    unsigned int randomPart = static_cast<unsigned int>(GetTickCount64() + (attempt * 977));
    wsprintfW(name, L"new-note-%08X%s", randomPart, ext.c_str());
    createdPath = JoinPath(tempDir, name);
    if (CreateEmptyFile(createdPath, nullptr)) return true;
    DWORD error = GetLastError();
    if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
      if (errorMessage) *errorMessage = L"Failed to create temporary note file: " + FormatWindowsErrorMessage(error);
      return false;
    }
  }

  if (errorMessage) *errorMessage = L"Could not allocate a temporary draft filename.";
  return false;
}

bool MoveTempNoteIntoGroup(const NoteGroupConfig& group, const std::wstring& sourcePath, std::wstring& finalPath, std::wstring* errorMessage) {
  if (group.type != NoteGroupType::Directory) {
    if (errorMessage) *errorMessage = L"Only directory groups support creating new notes.";
    return false;
  }
  DWORD attr = GetFileAttributesW(group.path.c_str());
  if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    if (errorMessage) *errorMessage = L"Group directory does not exist: " + group.path;
    return false;
  }

  std::wstring extension = L".txt";
  auto dot = sourcePath.find_last_of(L'.');
  if (dot != std::wstring::npos) extension = sourcePath.substr(dot);

  std::wstring baseName = ExtractNoteBaseName(sourcePath);
  if (!BuildUniqueTargetPath(group.path, baseName, extension, finalPath, errorMessage)) return false;

  if (!MoveFileExW(sourcePath.c_str(), finalPath.c_str(), MOVEFILE_COPY_ALLOWED)) {
    if (errorMessage) *errorMessage = L"Failed to move note into target directory: " + FormatWindowsErrorMessage(GetLastError());
    return false;
  }
  return true;
}

bool IsActionTargetCompatible(const ActionConfig& action, const NoteFile* file) {
  if (action.target == ActionTarget::Directory) {
    return file == nullptr;
  }
  return file != nullptr;
}

bool ExecuteAction(const ActionConfig& action, const NoteGroupConfig& group, const NoteFile* file,
  std::wstring* errorMessage, std::wstring* resolvedCommand) {
  HANDLE processHandle = nullptr;
  if (!StartActionProcess(action, group, file, processHandle, errorMessage, resolvedCommand)) return false;
  if (processHandle) CloseHandle(processHandle);
  return true;
}

bool ExecuteActionAndWait(const ActionConfig& action, const NoteGroupConfig& group, const NoteFile* file,
  std::wstring* errorMessage, std::wstring* resolvedCommand) {
  HANDLE processHandle = nullptr;
  if (!StartActionProcess(action, group, file, processHandle, errorMessage, resolvedCommand)) return false;
  if (!processHandle) {
    if (errorMessage) *errorMessage = L"Action started, but no process handle was returned for waiting.";
    return false;
  }
  WaitForSingleObject(processHandle, INFINITE);
  CloseHandle(processHandle);
  return true;
}
