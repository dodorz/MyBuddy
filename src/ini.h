#pragma once

#include <windows.h>

#include <string>
#include <unordered_map>
#include <vector>

class IniFile {
public:
  bool LoadUtf8(const std::wstring& path);

  std::wstring GetString(const std::wstring& section, const std::wstring& key,
    const std::wstring& def = L"") const;
  int GetInt(const std::wstring& section, const std::wstring& key, int def) const;
  bool GetBool(const std::wstring& section, const std::wstring& key, bool def) const;
  bool HasSection(const std::wstring& section) const;
  bool HasKey(const std::wstring& section, const std::wstring& key) const;
  // Section names in file order; the reference stays valid until the next load.
  const std::vector<std::wstring>& GetSectionNames() const { return sectionNames_; }

private:
  struct SectionEntry {
    std::wstring originalName;
    std::wstring normalizedName;
    std::vector<std::pair<std::wstring, std::wstring>> items;
    // Key lookup indexes are built while parsing so config reads stay O(1)
    // even for large config files.
    std::unordered_map<std::wstring, size_t> itemIndex{};
  };

  const SectionEntry* FindSection(const std::wstring& normalizedSection) const;

  std::vector<SectionEntry> sections_{};
  std::unordered_map<std::wstring, size_t> sectionIndex_{};
  std::vector<std::wstring> sectionNames_{};
};
