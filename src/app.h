#pragma once

#include "notes.h"

#include <windows.h>

#include <string>
#include <unordered_map>
#include <vector>

struct AppState {
  int version = 4;
  int dockEdge = 0;
  int x = 120;
  int y = 120;
  int w = 420;
  int h = 640;
  bool expanded = true;
  bool taskbarVisible = true;
};

class App {
public:
  int Run(HINSTANCE instance, int showCmd);

private:
  enum class DockEdge {
    None = 0,
    Left = 1,
    Right = 2,
    Top = 3,
  };

  struct Animation {
    bool active = false;
    bool expand = false;
    bool activateOnFinish = false;
    RECT from{};
    RECT to{};
    ULONGLONG startTick = 0;
    int durationMs = 0;
  };

  static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static LRESULT CALLBACK HotZoneWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static LRESULT CALLBACK ListBoxProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  LRESULT HandleMainMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  LRESULT HandleHotZoneMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  LRESULT HandleListBoxMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  bool LoadConfig();
  bool LoadState();
  void SaveState() const;
  void InitializeDefaultState();
  void ShowToTray();
  void ExitFromTray();
  void ShowTrayMenu(POINT pt);
  void ShowMainWindow(bool activate = true);
  void HideMainWindow();
  void ApplySavedGeometry();
  void SetTaskbarVisible(bool visible);
  void CreateHotZoneWindow();
  void DestroyHotZoneWindow();
  RECT GetWorkArea() const;
  RECT GetExpandedRect() const;
  RECT GetCollapsedRect() const;
  RECT GetHotZoneRect() const;
  void UpdateDockEdgeFromRect(RECT& rect);
  void SyncHotZone();
  void RequestExpand(bool activate);
  void RequestCollapse();
  void BeginAnimation(bool expand, bool activateOnFinish);
  void TickAnimation();
  void FinishAnimation();
  void CommitVisibleRect(const RECT& rect, bool detectDock);
  bool IsDocked() const;
  bool IsPointerInsideRect(const RECT& rect) const;
  void PollAutoHide();

  void CreateControls();
  void CreateFonts();
  void DestroyFonts();
  void LayoutControls();
  void RefreshNotes(const std::unordered_map<std::wstring, bool>* expandedStateByGroupId = nullptr);
  void RebuildVisibleRows();
  void InvalidateList();
  void DrawListItem(const DRAWITEMSTRUCT* dis);
  void HandleListLeftClick(POINT pt);
  void HandleListRightClick(POINT pt);
  int HitTestRow(POINT pt) const;
  RECT GetRowRect(int index) const;
  RECT GetGroupToggleRect(const RECT& rowRect) const;
  RECT GetGroupAddRect(const RECT& rowRect) const;
  RECT GetGroupClipboardRect(const RECT& rowRect) const;
  void ToggleGroup(int groupIndex);
  void CreateNoteForGroup(int groupIndex);
  void CreateNoteFromClipboardForGroup(int groupIndex);
  void OpenFileNote(int groupIndex, int fileIndex);
  void RunGroupMenu(int groupIndex, POINT screenPt);
  void RunFileMenu(int groupIndex, int fileIndex, POINT screenPt);
  void ReloadConfigAndRefreshNotes();
  void RefreshGroup(int groupIndex);

  std::wstring GetAppDataDir() const;
  std::wstring GetProgramConfigPath() const;
  std::wstring GetFallbackConfigPath() const;
  std::wstring GetStatePath() const;

  HINSTANCE instance_ = nullptr;
  HANDLE singleInstanceMutex_ = nullptr;
  HWND hwnd_ = nullptr;
  HWND hotZone_ = nullptr;
  HWND listBox_ = nullptr;
  WNDPROC originalListBoxProc_ = nullptr;
  HFONT fontBody_ = nullptr;
  HFONT fontGroup_ = nullptr;
  HFONT fontMeta_ = nullptr;
  HFONT fontSymbol_ = nullptr;
  AppState state_{};
  NotesConfig notesConfig_{};
  std::vector<std::vector<NoteFile>> notesByGroup_{};
  std::vector<NoteGroupLoadState> groupStates_{};
  std::vector<bool> expandedGroups_{};
  std::vector<VisibleRow> visibleRows_{};
  std::wstring globalStatusMessage_{};
  std::wstring hotKeySpec_ = L"Ctrl+Alt+B";
  UINT hotKeyModifiers_ = MOD_CONTROL | MOD_ALT;
  UINT hotKeyVk_ = 'B';
  Animation animation_{};
  ULONGLONG lastPointerInsideTick_ = 0;
  bool suppressWindowTracking_ = false;
  bool inMoveSize_ = false;
  bool trayHidden_ = false;
  bool hotZoneVisible_ = false;
  bool stateLoaded_ = false;
};
