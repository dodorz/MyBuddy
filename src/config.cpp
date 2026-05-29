#include "config.h"

#include <windows.h>

static std::wstring ReadString(const std::wstring& path, const wchar_t* section, const wchar_t* key, const wchar_t* def) {
  wchar_t buffer[260];
  GetPrivateProfileStringW(section, key, def, buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
  return buffer;
}

static int ReadInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int def) {
  return GetPrivateProfileIntW(section, key, def, path.c_str());
}

static bool ReadBool(const std::wstring& path, const wchar_t* section, const wchar_t* key, bool def) {
  return ReadInt(path, section, key, def ? 1 : 0) != 0;
}

void LoadAppConfig(AppConfig& cfg, const std::wstring& programConfig, const std::wstring& fallbackConfig) {
  const std::wstring& path = GetFileAttributesW(programConfig.c_str()) != INVALID_FILE_ATTRIBUTES ? programConfig : fallbackConfig;
  cfg.edgePeekPx = ReadInt(path, L"ui", L"edgePeekPx", cfg.edgePeekPx);
  cfg.slideMs = ReadInt(path, L"ui", L"slideMs", cfg.slideMs);
  cfg.autoHide = ReadBool(path, L"ui", L"autoHide", cfg.autoHide);
  cfg.appName = ReadString(path, L"meta", L"appName", cfg.appName.c_str());
}

void SaveAppConfig(const AppConfig& cfg, const std::wstring& path) {
  WritePrivateProfileStringW(L"meta", L"appName", cfg.appName.c_str(), path.c_str());
  WritePrivateProfileStringW(L"ui", L"edgePeekPx", std::to_wstring(cfg.edgePeekPx).c_str(), path.c_str());
  WritePrivateProfileStringW(L"ui", L"slideMs", std::to_wstring(cfg.slideMs).c_str(), path.c_str());
  WritePrivateProfileStringW(L"ui", L"autoHide", cfg.autoHide ? L"1" : L"0", path.c_str());
}
