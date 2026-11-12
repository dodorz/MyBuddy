#pragma once

// One-time startup setup: resolves the log/dump paths and installs the
// unhandled exception filter. Call it first in wWinMain, before any window
// code runs, because that is exactly where startup failures happen.
void InstallCrashHandler();

// Appends one timestamped line to the diagnostics log. Safe to call on the
// failure paths of App::Run(); never throws and never shows UI.
void LogDiagnostic(const wchar_t* message);

// Path of the log file, or L"" when diagnostics could not be set up.
const wchar_t* GetDiagnosticsLogPath();
