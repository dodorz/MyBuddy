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

enum class ActionTarget {
  File,
  Directory,
};

struct ActionConfig {
  std::wstring id;
  std::wstring title;
  std::wstring command;
  ActionTarget target = ActionTarget::File;
};

struct NoteGroupConfig {
  std::wstring id;
  std::wstring title;
  std::wstring path;
  bool expanded = true;
  bool showExtensions = false;
  std::vector<std::wstring> filePatterns;
  std::wstring createExtension;
  int maxItems = 5;
  NoteSortBy sortBy = NoteSortBy::ModifiedTime;
  SortOrder sortOrder = SortOrder::Desc;
  std::wstring defaultFileAction;
  std::vector<std::wstring> fileActions;
  std::vector<std::wstring> groupActions;
};

struct NotesConfig {
  std::vector<std::wstring> defaultFilePatterns{L"*.txt", L"*.md"};
  std::wstring defaultCreateExtension;
  int defaultMaxItems = 5;
  NoteSortBy defaultSortBy = NoteSortBy::ModifiedTime;
  SortOrder defaultSortOrder = SortOrder::Desc;
  bool defaultGroupExpanded = true;
  bool defaultShowExtensions = false;
  std::wstring defaultFileAction;
  std::vector<std::wstring> defaultFileActions;
  std::vector<std::wstring> defaultGroupActions;
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
bool CreateTempNoteForGroup(const NoteGroupConfig& group, std::wstring& createdPath, std::wstring* errorMessage = nullptr);
bool MoveTempNoteIntoGroup(const NoteGroupConfig& group, const std::wstring& sourcePath, std::wstring& finalPath, std::wstring* errorMessage = nullptr);
bool IsActionTargetCompatible(const ActionConfig& action, const NoteFile* file);
bool ExecuteAction(const ActionConfig& action, const NoteGroupConfig& group, const NoteFile* file,
  std::wstring* errorMessage = nullptr, std::wstring* resolvedCommand = nullptr);
bool ExecuteActionAndWait(const ActionConfig& action, const NoteGroupConfig& group, const NoteFile* file,
  std::wstring* errorMessage = nullptr, std::wstring* resolvedCommand = nullptr);
