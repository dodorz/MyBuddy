#pragma once

#include "app.h"

void LoadAppConfig(AppConfig& cfg, const std::wstring& programConfig, const std::wstring& fallbackConfig);
void SaveAppConfig(const AppConfig& cfg, const std::wstring& path);
