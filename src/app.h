#pragma once

#include <windows.h>
#include <string>

struct AppConfig {
  std::wstring appName = L"MyBuddy";
  int edgePeekPx = 8;
  int slideMs = 160;
  bool autoHide = true;
};

struct AppState {
  int version = 1;
  int dockEdge = 1;
  int x = 0;
  int y = 0;
  int w = 360;
  int h = 520;
  bool expanded = false;
};

class App {
public:
  int Run(HINSTANCE instance, int showCmd);

private:
  enum class WindowKind { Main, HotZone };
  enum class DockEdge { Left = 0, Right = 1, Top = 2, Bottom = 3 };

  struct Animation {
    bool active = false;
    bool targetExpanded = false;
    RECT from{};
    RECT to{};
    ULONGLONG startTick = 0;
  };

  static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static LRESULT CALLBACK HotZoneWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  LRESULT HandleMainMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  LRESULT HandleHotZoneMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  bool LoadConfig();
  bool LoadState();
  void SaveState() const;

  void CreateHotZoneWindow();
  void DestroyHotZoneWindow();
  void SyncGeometry(bool expanded, bool animate);
  RECT GetTargetRect(bool expanded) const;
  RECT GetHiddenRect() const;
  void UpdateHotZonePlacement();
  void ShowHotZone(bool show);
  void RequestExpand();
  void RequestCollapse();
  void BeginAnimation(bool expanded);
  void TickAnimation();
  void FinishAnimation();
  void SetDockEdgeFromRect(const RECT& rc);
  RECT GetWorkArea() const;
  bool IsPointerInsideMain() const;
  bool IsPointerInsideHotZone() const;
  void ArmCollapseTimer();
  void DisarmCollapseTimer();
  void ShowMainWindow();
  void HideMainWindow();
  void ApplySavedGeometry();

  std::wstring GetAppDataDir() const;
  std::wstring GetProgramConfigPath() const;
  std::wstring GetFallbackConfigPath() const;
  std::wstring GetStatePath() const;

  HWND hwnd_ = nullptr;
  HWND hotZone_ = nullptr;
  HWND status_ = nullptr;
  HINSTANCE instance_ = nullptr;
  AppConfig config_{};
  AppState state_{};
  Animation animation_{};
  bool trackingLeave_ = false;
  bool hotZoneVisible_ = false;
  bool closing_ = false;
  UINT_PTR collapseTimer_ = 1;
  UINT_PTR animationTimer_ = 2;
  RECT currentRect_{};
};
