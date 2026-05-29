#pragma once

#include "app.h"

bool LoadAppState(AppState& state, const std::wstring& path);
void SaveAppState(const AppState& state, const std::wstring& path);
