#include "config.h"

#include <windows.h>

static std::wstring ReadString(const std::wstring& path, const wchar_t* section, const wchar_t* key, const wchar_t* def) {
  wchar_t buffer[260];
  GetPrivateProfileStringW(section, key, def, buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
  return buffer;
}

void LoadAppConfig(AppConfig& cfg, const std::wstring& programConfig, const std::wstring& fallbackConfig) {
  const std::wstring& path = GetFileAttributesW(programConfig.c_str()) != INVALID_FILE_ATTRIBUTES ? programConfig : fallbackConfig;
  cfg.appName = ReadString(path, L"meta", L"appName", cfg.appName.c_str());
}

void SaveAppConfig(const AppConfig& cfg, const std::wstring& path) {
  WritePrivateProfileStringW(L"meta", L"appName", cfg.appName.c_str(), path.c_str());
}
