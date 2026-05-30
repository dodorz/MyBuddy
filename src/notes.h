#pragma once

#include <windows.h>

#include <map>
#include <string>
#include <vector>

enum class NoteSortBy {
  Name,
  CreatedTime,
  ModifiedTime,
};

enum class SortOrder {
  Asc,
  Desc,
};

enum class NoteGroupLoadState {
  Ok,
  Empty,
  MissingDirectory,
};

struct ActionConfig {
  std::wstring id;
  std::wstring title;
  std::wstring command;
};

struct NoteGroupConfig {
  std::wstring id;
  std::wstring title;
  std::wstring path;
  bool expanded = true;
  std::vector<std::wstring> filePatterns;
  int maxItems = 5;
  NoteSortBy sortBy = NoteSortBy::ModifiedTime;
  SortOrder sortOrder = SortOrder::Desc;
  std::wstring defaultFileAction;
  std::vector<std::wstring> fileActions;
  std::vector<std::wstring> groupActions;
};

struct NotesConfig {
  std::vector<std::wstring> defaultFilePatterns{L"*.txt", L"*.md"};
  int defaultMaxItems = 5;
  NoteSortBy defaultSortBy = NoteSortBy::ModifiedTime;
  SortOrder defaultSortOrder = SortOrder::Desc;
  bool defaultGroupExpanded = true;
  std::map<std::wstring, ActionConfig> actions;
  std::vector<NoteGroupConfig> groups;
};

struct NoteFile {
  std::wstring path;
  std::wstring dir;
  std::wstring name;
  std::wstring stem;
  FILETIME createdTime{};
  FILETIME modifiedTime{};
};

struct VisibleRow {
  enum class Type {
    Group,
    File,
    GroupMessage,
    GlobalMessage,
  };

  Type type = Type::Group;
  int groupIndex = -1;
  int fileIndex = -1;
};

bool LoadNotesConfig(const std::wstring& path, NotesConfig& config);
void LoadNoteFiles(const NoteGroupConfig& group, std::vector<NoteFile>& files, NoteGroupLoadState* state = nullptr);
bool CreateNoteInGroup(const NoteGroupConfig& group, std::wstring& createdPath, std::wstring* errorMessage = nullptr);
bool ExecuteAction(const ActionConfig& action, const NoteGroupConfig& group, const NoteFile* file,
  std::wstring* errorMessage = nullptr, std::wstring* resolvedCommand = nullptr);
