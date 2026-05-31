#include "state.h"

#include <windows.h>
#include <cstring>

namespace {
struct StateBlobV4 {
  char magic[8] = {'M','B','S','T','A','T','E','4'};
  int version = 4;
  int dockEdge = 0;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  int expanded = 1;
  int taskbarVisible = 1;
};

struct StateBlobV3 {
  char magic[8] = {'M','B','S','T','A','T','E','3'};
  int version = 3;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  int taskbarVisible = 1;
};

struct LegacyStateBlobV2 {
  char magic[8] = {'M','B','S','T','A','T','E','2'};
  int version = 2;
  int dockEdge = 0;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  int expanded = 0;
  int taskbarVisible = 1;
};
}

bool LoadAppState(AppState& state, const std::wstring& path) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  LegacyStateBlobV2 blob{};
  DWORD read = 0;
  BOOL ok = ReadFile(h, &blob, sizeof(blob), &read, nullptr);
  CloseHandle(h);
  if (!ok || read < sizeof(StateBlobV3) || std::memcmp(blob.magic, "MBSTATE", 7) != 0) return false;

  if (blob.version >= 4 && read >= sizeof(StateBlobV4)) {
    const StateBlobV4* current = reinterpret_cast<const StateBlobV4*>(&blob);
    state.version = current->version;
    state.dockEdge = current->dockEdge;
    state.x = current->x;
    state.y = current->y;
    state.w = current->w;
    state.h = current->h;
    state.expanded = current->expanded != 0;
    state.taskbarVisible = current->taskbarVisible != 0;
    return true;
  }

  if (blob.version == 3 && read >= sizeof(StateBlobV3)) {
    const StateBlobV3* v3 = reinterpret_cast<const StateBlobV3*>(&blob);
    state.version = 4;
    state.dockEdge = 0;
    state.x = v3->x;
    state.y = v3->y;
    state.w = v3->w;
    state.h = v3->h;
    state.expanded = true;
    state.taskbarVisible = v3->taskbarVisible != 0;
    return true;
  }

  state.version = 4;
  state.dockEdge = blob.dockEdge;
  state.x = blob.x;
  state.y = blob.y;
  state.w = blob.w;
  state.h = blob.h;
  state.expanded = blob.expanded != 0;
  state.taskbarVisible = blob.taskbarVisible != 0;
  return true;
}

void SaveAppState(const AppState& state, const std::wstring& path) {
  StateBlobV4 blob{};
  blob.version = state.version;
  blob.dockEdge = state.dockEdge;
  blob.x = state.x;
  blob.y = state.y;
  blob.w = state.w;
  blob.h = state.h;
  blob.expanded = state.expanded ? 1 : 0;
  blob.taskbarVisible = state.taskbarVisible ? 1 : 0;
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  DWORD written = 0;
  WriteFile(h, &blob, sizeof(blob), &written, nullptr);
  CloseHandle(h);
}
