#include "app.h"
#include "diagnostics.h"

#include <string>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int showCmd) {
  // Installed before any window code runs: startup failures happen inside
  // WM_CREATE, where nothing else can report them.
  InstallCrashHandler();
  const std::wstring logPath = GetDiagnosticsLogPath();
  LogDiagnostic((L"startup: wWinMain entered, log=" + logPath).c_str());

  App app;
  const int result = app.Run(instance, showCmd);

  LogDiagnostic((L"startup: Run() returned " + std::to_wstring(result)).c_str());
  return result;
}
