#pragma once

#include <windows.h>

#include <string>
#include <vector>

class IniFile {
public:
  bool LoadUtf8(const std::wstring& path);

  std::wstring GetString(const std::wstring& section, const std::wstring& key,
    const std::wstring& def = L"") const;
  int GetInt(const std::wstring& section, const std::wstring& key, int def) const;
  bool GetBool(const std::wstring& section, const std::wstring& key, bool def) const;
  std::vector<std::wstring> GetSectionNames() const;

private:
  std::vector<std::wstring> sectionNames_{};
  std::vector<std::pair<std::wstring, std::vector<std::pair<std::wstring, std::wstring>>>> sections_{};
};
