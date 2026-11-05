#pragma once

#include "ini.h"

#include <windows.h>

#include <map>
#include <string>
#include <vector>

enum class NoteSortBy {
  LineOrder,
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
  MissingPath,
};

enum class ActionTarget {
  File,
  Directory,
  Both,
};

enum class NoteGroupType {
  Directory,
  TextLines,
  TodoTxt,
};

struct ActionConfig {
  std::wstring id;
  std::wstring title;
  std::wstring command;
  ActionTarget target = ActionTarget::File;
  int showConsole = -1;
};

struct NoteGroupConfig {
  std::wstring id;
  std::wstring title;
  std::wstring path;
  NoteGroupType type = NoteGroupType::Directory;
  bool expanded = true;
  bool showExtensions = false;
  bool showSubdir = false;
  std::vector<std::wstring> hideSubdirPatterns;
  std::vector<std::wstring> filePatterns;
  std::wstring createExtension;
  int maxItems = 5;
  int maxPinned = 3;
  NoteSortBy sortBy = NoteSortBy::ModifiedTime;
  SortOrder sortOrder = SortOrder::Desc;
  std::wstring defaultItemAction;
  std::wstring deleteTitle;
  std::wstring deleteCommand;
  std::vector<std::wstring> itemActions;
  std::vector<std::wstring> groupActions;
};

struct NoteGroupDefaults {
  bool showExtensions = false;
  bool showSubdir = false;
  std::vector<std::wstring> hideSubdirPatterns;
  std::vector<std::wstring> filePatterns{L"*.txt", L"*.md"};
  std::wstring createExtension;
  int maxItems = 5;
  int maxPinned = 3;
  NoteSortBy sortBy = NoteSortBy::ModifiedTime;
  SortOrder sortOrder = SortOrder::Desc;
  std::wstring defaultItemAction;
  std::wstring deleteTitle;
  std::wstring deleteCommand;
  std::vector<std::wstring> itemActions;
  std::vector<std::wstring> groupActions;
};

struct NotesConfig {
  bool defaultGroupExpanded = true;
  NoteGroupDefaults sharedDefaults;
  NoteGroupDefaults dirDefaults;
  NoteGroupDefaults textDefaults;
  NoteGroupDefaults todoDefaults;
  std::map<std::wstring, ActionConfig> actions;
  std::vector<NoteGroupConfig> groups;
};

// Display-name / pin information decoded once per Markdown file.
struct MarkdownMetadata {
  std::wstring title;
  std::wstring author;
  // First level-1 heading, used when the front matter has no title.
  std::wstring headingTitle;
  bool pinned = false;
  bool hasFrontMatter = false;
  bool hasTitle = false;
  // The probe already covered the whole file, so fallbacks are authoritative.
  bool scannedWholeFile = false;
  // The probe found a definitive display name (or covered the whole file).
  bool nameResolved = false;
};

struct NoteFile {
  std::wstring path;
  std::wstring dir;
  std::wstring name;
  std::wstring stem;
  std::wstring displayName;
  std::wstring itemText;
  int lineNumber = 0;
  FILETIME createdTime{};
  FILETIME modifiedTime{};
  ULONGLONG size = 0;
  // Front matter already decoded while probing this file; reused by the list
  // to build the display name without reading the file a second time.
  MarkdownMetadata metadata{};
};

struct VisibleRow {
  enum class Type {
    Group,
    Subdir,
    File,
    GroupMessage,
    GlobalMessage,
  };

  Type type = Type::Group;
  int groupIndex = -1;
  int subdirIndex = -1;
  int fileIndex = -1;
};

// Parses the notes sections of an already loaded config file. The startup path
// loads config.ini once and feeds the same IniFile to both parsers.
void LoadNotesConfigFromIni(const IniFile& ini, NotesConfig& config);
void LoadNoteFiles(const NoteGroupConfig& group, std::vector<NoteFile>& files, NoteGroupLoadState* state = nullptr,
  bool ignoreMaxItems = false);
// Drops cached entries for one path after the app rewrote that note file.
void InvalidateMarkdownMetadata(const std::wstring& path);
bool IsMarkdownNoteFile(const NoteFile& file);
bool IsNoteFilePinned(const NoteFile& file, bool& pinned, std::wstring* errorMessage = nullptr);
bool SetNoteFilePinned(const NoteFile& file, bool pinned, std::wstring* errorMessage = nullptr);
bool ToggleMarkdownCheckbox(const NoteFile& file, std::wstring* errorMessage = nullptr);
bool ToggleTodoTxtTask(const NoteFile& file, std::wstring* errorMessage = nullptr);
bool MoveTextFileLine(const NoteFile& file, int targetLineNumber, bool insertAfter,
  std::wstring* errorMessage = nullptr);
bool MoveNoteFileToDirectory(const NoteFile& file, const std::wstring& targetDirectory,
  std::wstring* errorMessage = nullptr);
bool CreateNoteInGroup(const NoteGroupConfig& group, std::wstring& createdPath, std::wstring* errorMessage = nullptr);
bool CreateTempNoteForGroup(const NoteGroupConfig& group, std::wstring& createdPath, std::wstring* errorMessage = nullptr);
bool MoveTempNoteIntoGroup(const NoteGroupConfig& group, const std::wstring& sourcePath, std::wstring& finalPath, std::wstring* errorMessage = nullptr);
bool IsActionTargetCompatible(const ActionConfig& action, const NoteFile* file);
bool ExecuteAction(const ActionConfig& action, const NoteGroupConfig& group, const NoteFile* file,
  std::wstring* errorMessage = nullptr, std::wstring* resolvedCommand = nullptr);
bool ExecuteActionAndWait(const ActionConfig& action, const NoteGroupConfig& group, const NoteFile* file,
  std::wstring* errorMessage = nullptr, std::wstring* resolvedCommand = nullptr);
bool StartActionProcessTracked(const ActionConfig& action, const NoteGroupConfig& group, const NoteFile* file,
  HANDLE& processHandle, std::wstring* errorMessage = nullptr, std::wstring* resolvedCommand = nullptr);
