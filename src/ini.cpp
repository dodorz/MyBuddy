#include "ini.h"

#include <algorithm>
#include <fstream>

namespace {
std::wstring Trim(std::wstring value) {
  size_t start = 0;
  while (start < value.size() && iswspace(value[start])) ++start;
  size_t end = value.size();
  while (end > start && iswspace(value[end - 1])) --end;
  return value.substr(start, end - start);
}

std::wstring ToLower(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(), towlower);
  return value;
}

std::wstring DecodeUtf8(const std::string& bytes) {
  if (bytes.empty()) return L"";
  int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
    static_cast<int>(bytes.size()), nullptr, 0);
  if (length <= 0) return L"";

  std::wstring wide(static_cast<size_t>(length), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
      static_cast<int>(bytes.size()), wide.data(), length) <= 0) {
    return L"";
  }
  return wide;
}
}  // namespace

bool IniFile::LoadUtf8(const std::wstring& path) {
  sections_.clear();

  std::ifstream input(path, std::ios::binary);
  if (!input) return false;

  std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (bytes.size() >= 3 &&
      static_cast<unsigned char>(bytes[0]) == 0xEF &&
      static_cast<unsigned char>(bytes[1]) == 0xBB &&
      static_cast<unsigned char>(bytes[2]) == 0xBF) {
    bytes.erase(0, 3);
  }

  std::wstring content = DecodeUtf8(bytes);
  if (!bytes.empty() && content.empty()) return false;

  std::wstring currentSection;
  size_t start = 0;
  while (start <= content.size()) {
    size_t end = content.find_first_of(L"\r\n", start);
    std::wstring line = end == std::wstring::npos
      ? content.substr(start)
      : content.substr(start, end - start);
    line = Trim(line);

    if (!line.empty() && line[0] != L';' && line[0] != L'#') {
      if (line.front() == L'[' && line.back() == L']' && line.size() >= 2) {
        const std::wstring originalSection = Trim(line.substr(1, line.size() - 2));
        currentSection = ToLower(originalSection);
        bool known = false;
        for (const SectionEntry& section : sections_) {
          if (section.normalizedName == currentSection) {
            known = true;
            break;
          }
        }
        if (!known) {
          sections_.push_back({originalSection, currentSection, {}});
        }
      } else {
        size_t split = line.find(L'=');
        if (split != std::wstring::npos && !currentSection.empty()) {
          std::wstring key = ToLower(Trim(line.substr(0, split)));
          std::wstring value = Trim(line.substr(split + 1));
          for (auto& section : sections_) {
            if (section.normalizedName != currentSection) continue;
            bool updated = false;
            for (auto& item : section.items) {
              if (item.first == key) {
                item.second = value;
                updated = true;
                break;
              }
            }
            if (!updated) {
              section.items.push_back({key, value});
            }
            break;
          }
        }
      }
    }

    if (end == std::wstring::npos) break;
    if (content[end] == L'\r' && end + 1 < content.size() && content[end + 1] == L'\n') {
      start = end + 2;
    } else {
      start = end + 1;
    }
  }

  return true;
}

std::wstring IniFile::GetString(const std::wstring& section, const std::wstring& key, const std::wstring& def) const {
  const std::wstring normalizedSection = ToLower(section);
  const std::wstring normalizedKey = ToLower(key);
  for (const auto& entry : sections_) {
    if (entry.normalizedName != normalizedSection) continue;
    for (const auto& item : entry.items) {
      if (item.first == normalizedKey) {
        return item.second;
      }
    }
    return def;
  }
  return def;
}

int IniFile::GetInt(const std::wstring& section, const std::wstring& key, int def) const {
  std::wstring value = GetString(section, key);
  if (value.empty()) return def;
  try {
    size_t used = 0;
    int result = std::stoi(value, &used, 10);
    if (used != value.size()) return def;
    return result;
  } catch (...) {
    return def;
  }
}

bool IniFile::GetBool(const std::wstring& section, const std::wstring& key, bool def) const {
  return GetInt(section, key, def ? 1 : 0) != 0;
}

bool IniFile::HasSection(const std::wstring& section) const {
  const std::wstring normalizedSection = ToLower(section);
  for (const auto& entry : sections_) {
    if (entry.normalizedName == normalizedSection) return true;
  }
  return false;
}

std::vector<std::wstring> IniFile::GetSectionNames() const {
  std::vector<std::wstring> names;
  names.reserve(sections_.size());
  for (const SectionEntry& section : sections_) {
    names.push_back(section.originalName);
  }
  return names;
}
