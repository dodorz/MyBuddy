#include "state.h"

#include <windows.h>
#include <cstring>

namespace {
struct StateBlob {
  char magic[8] = {'M','B','S','T','A','T','E','1'};
  int version = 1;
  int dockEdge = 0;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  int expanded = 0;
};
}

bool LoadAppState(AppState& state, const std::wstring& path) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  StateBlob blob{};
  DWORD read = 0;
  BOOL ok = ReadFile(h, &blob, sizeof(blob), &read, nullptr);
  CloseHandle(h);
  if (!ok || read != sizeof(blob) || std::memcmp(blob.magic, "MBSTATE1", 8) != 0) return false;
  state.version = blob.version;
  state.dockEdge = blob.dockEdge;
  state.x = blob.x;
  state.y = blob.y;
  state.w = blob.w;
  state.h = blob.h;
  state.expanded = blob.expanded != 0;
  return true;
}

void SaveAppState(const AppState& state, const std::wstring& path) {
  StateBlob blob{};
  blob.version = state.version;
  blob.dockEdge = state.dockEdge;
  blob.x = state.x;
  blob.y = state.y;
  blob.w = state.w;
  blob.h = state.h;
  blob.expanded = state.expanded ? 1 : 0;
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  DWORD written = 0;
  WriteFile(h, &blob, sizeof(blob), &written, nullptr);
  CloseHandle(h);
}
