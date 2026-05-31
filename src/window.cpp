#include "window.h"

#include "app.h"

HWND CreateMainWindow(HINSTANCE instance, void* app) {
  (void)app;
  return CreateWindowExW(
    WS_EX_APPWINDOW,
    L"MyBuddyWindowClass",
    L"MyBuddy",
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    360,
    520,
    nullptr,
    nullptr,
    instance,
    app
  );
}
