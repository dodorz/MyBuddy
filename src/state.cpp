#include "state.h"

#include <windows.h>
#include <cstring>

namespace {
struct StateBlob {
  char magic[8] = {'M','B','S','T','A','T','E','3'};
  int version = 3;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  int taskbarVisible = 1;
};

struct LegacyStateBlob {
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
  LegacyStateBlob blob{};
  DWORD read = 0;
  BOOL ok = ReadFile(h, &blob, sizeof(blob), &read, nullptr);
  CloseHandle(h);
  if (!ok || read < sizeof(StateBlob) || std::memcmp(blob.magic, "MBSTATE", 7) != 0) return false;

  const StateBlob* current = reinterpret_cast<const StateBlob*>(&blob);
  if (current->version >= 3 && read >= sizeof(StateBlob)) {
    state.version = current->version;
    state.x = current->x;
    state.y = current->y;
    state.w = current->w;
    state.h = current->h;
    state.taskbarVisible = current->taskbarVisible != 0;
    return true;
  }

  state.version = 3;
  state.x = blob.x;
  state.y = blob.y;
  state.w = blob.w;
  state.h = blob.h;
  state.taskbarVisible = read >= sizeof(blob) ? blob.taskbarVisible != 0 : true;
  return true;
}

void SaveAppState(const AppState& state, const std::wstring& path) {
  StateBlob blob{};
  blob.version = state.version;
  blob.x = state.x;
  blob.y = state.y;
  blob.w = state.w;
  blob.h = state.h;
  blob.taskbarVisible = state.taskbarVisible ? 1 : 0;
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  DWORD written = 0;
  WriteFile(h, &blob, sizeof(blob), &written, nullptr);
  CloseHandle(h);
}
