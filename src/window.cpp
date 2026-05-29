#include "window.h"

#include "app.h"

HWND CreateMainWindow(HINSTANCE instance, void* app) {
  (void)app;
  return CreateWindowExW(
    WS_EX_APPWINDOW,
    L"MyBuddyWindowClass",
    L"MyBuddy",
    WS_POPUP | WS_THICKFRAME | WS_CAPTION,
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
