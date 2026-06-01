# Notes Configuration

This document defines the `config.ini` format for the notes feature.

## Overview

- Each note group maps either to one directory or to one text file.
- Directory groups use files as items.
- `text` groups use non-empty lines in one text file as items.
- If a `text` group points to a Markdown file and that file starts with TOML front matter, the front matter block is ignored.
- Groups are shown together in one list.
- Group headers are collapsible.
- The add button belongs to the group header.
- File actions and group actions are both defined in config.

## App Section

Section name: `[app]`

Supported keys:

- `globalHotKey`
  - Global hotkey used to show the window
  - Default: `Ctrl+Alt+B`
  - Simple supported forms: `Ctrl+Alt+B`, `Ctrl+Shift+1`, `Alt+F6`

## Example

```ini
[app]
globalHotKey=Ctrl+Alt+B

[notes_default]
maxItems=5
defaultGroupExpanded=1
showExtensions=0
defaultFileAction=Edit
fileActions=Edit;reveal
groupActions=terminal

[notes_dir_default]
filePatterns=*.txt;*.md
createExtension=.md
sortBy=mtime
sortOrder=desc

[notes_text_default]
sortBy=line
sortOrder=asc

[file_action.Edit]
title=编辑
command="C:\Program Files\Notepad++\notepad++.exe" "{file}"

[file_action.reveal]
title=打开目录
command=explorer.exe /select,"{file}"

[dir_action.terminal]
title=终端打开本组
command=wt.exe -d "{group_dir}"

[note_group.todo]
title=待办
type=text
path=D:\Notes\todo.txt
sortBy=line
defaultFileAction=Edit
fileActions=reveal
groupActions=terminal

[note_group.work]
title=工作
path=D:\Notes\Work
expanded=1
filePatterns=*.md
createExtension=.md
maxItems=8
sortBy=name
sortOrder=asc
showExtensions=0
defaultFileAction=Edit
fileActions=Edit;reveal
groupActions=terminal

[note_group.personal]
title=个人
path=D:\Notes\Personal
expanded=0
defaultFileAction=Edit
fileActions=Edit;reveal
groupActions=
```

## Default Section

Section name: `[notes_default]`

Supported keys:

- `maxItems`
  - Max visible note items per group.
  - `0` means unlimited
  - Default: `5`
- `defaultGroupExpanded`
  - `1` or `0`
  - Default: `1`
- `showExtensions`
  - Whether note list items show filename extensions by default
  - Supported: `1`, `0`
  - Default: `0`
- `defaultFileAction`
  - Default file-item action inherited by groups
  - Must reference a `file_action`
- `fileActions`
  - Default file-item context menu actions inherited by groups
  - Must reference `file_action` ids
- `groupActions`
  - Default group-header context menu actions inherited by groups
  - Must reference `dir_action` ids

## Type Default Sections

Optional section names:

- `[notes_dir_default]`
- `[notes_text_default]`

These sections inherit from `[notes_default]`, then `[note_group.<id>]` can override again.

Supported keys for `[notes_dir_default]`:

- `filePatterns`
  - Semicolon-separated patterns.
  - Default: inherits `[notes_default]`, otherwise `*.txt;*.md`
- `createExtension`
  - Default extension used when creating a new note.
  - Examples: `.txt`, `.md`
  - If missing or empty, falls back to the first writable extension from `filePatterns`
- `maxItems`
  - Max visible note items per group.
  - `0` means unlimited
  - Default: inherits `[notes_default]`, otherwise `5`
- `sortBy`
  - Supported: `name`, `ctime`, `mtime`
  - Default: `mtime`
- `sortOrder`
  - Supported: `asc`, `desc`
  - Default: `desc`
- `showExtensions`
  - Whether note list items show filename extensions by default
  - Supported: `1`, `0`
  - Default: inherits `[notes_default]`, otherwise `0`
- `defaultFileAction`
  - Default file-item action inherited by dir groups
- `fileActions`
  - Default file-item context menu actions inherited by dir groups
- `groupActions`
  - Default group-header context menu actions inherited by dir groups

Supported keys for `[notes_text_default]`:

- `maxItems`
  - Max visible text items per group.
  - `0` means unlimited
  - Default: inherits `[notes_default]`, otherwise `5`
- `sortBy`
  - Supported: `line`, `name`, `ctime`, `mtime`
  - Default: `line`
- `sortOrder`
  - Supported: `asc`, `desc`
  - Default: `asc`
- `showExtensions`
  - Supported: `1`, `0`
  - Default: inherits `[notes_default]`, otherwise `0`
- `defaultFileAction`
  - Default file-item action inherited by text groups
- `fileActions`
  - Default file-item context menu actions inherited by text groups
- `groupActions`
  - Default group-header context menu actions inherited by text groups

## Action Sections

Section formats:

- `[file_action.<id>]`
- `[dir_action.<id>]`

`file_action` is for file items only.  
`dir_action` is for group or directory actions only.

Supported keys:

- `title`
  - User-facing menu text.
  - If missing, defaults to the action id.
- `command`
  - Full command template.
  - Executed via `ShellExecuteExW`.
  - If shell behavior is needed, explicitly configure `cmd.exe /c ...`.

Example:

```ini
[file_action.Edit]
title=编辑
command="C:\Program Files\Notepad++\notepad++.exe" "{file}"
```

## Group Sections

Section format: `[note_group.<id>]`

Required keys:

- `path`
  - `directory` groups: one directory path
  - `text` groups: one text file path

Optional keys:

- `type`
  - Supported: `dir`, `text`
  - Default: `dir`
  - `dir`: path is a directory, files become items
  - `text`: path is one text file, non-empty lines become items

- `title`
  - If missing, defaults to the group id from `[note_group.<id>]`

- `expanded`
  - `1` or `0`
  - If missing, inherits `defaultGroupExpanded`
- `showExtensions`
  - `1` or `0`
  - If missing, inherits typed default `showExtensions`
- `filePatterns`
  - If missing, inherits typed default `filePatterns`
- `createExtension`
  - If missing, inherits typed default `createExtension`
- `maxItems`
  - If missing, inherits typed default `maxItems`
  - `0` means unlimited
- `sortBy`
  - If missing, inherits typed default `sortBy`
- `sortOrder`
  - If missing, inherits typed default `sortOrder`
- `defaultFileAction`
  - Must reference a `file_action`
  - If missing, inherits typed default `defaultFileAction`
  - Action id used for double-click or primary open behavior
- `fileActions`
  - Must reference `file_action` ids
  - If missing, inherits typed default `fileActions`
  - Semicolon-separated action ids shown in file item context menu
- `groupActions`
  - Must reference `dir_action` ids
  - If missing, inherits typed default `groupActions`
  - Semicolon-separated action ids shown in group header context menu

## Sorting Rules

- Directory groups are filtered first.
- Items are sorted second.
- `maxItems` is applied last.
- `maxItems=0` disables item count truncation.
- If `sortBy=line`, items keep source line order.
- If `sortBy=name`, secondary sort is `mtime desc`.
- If `sortBy=ctime`, secondary sort is `name asc`.
- If `sortBy=mtime`, secondary sort is `name asc`.

## File Matching

- `filePatterns` accepts shell-style patterns such as `*.txt` and `*.md`.
- Matching is case-insensitive.
- Only files are included, not subdirectories.
- First implementation should enumerate one directory level only.
- `text` groups do not use `filePatterns`.
- For Markdown source files, leading TOML front matter is skipped before line items are generated.

## Add Note Behavior

- The add button on a group header first creates one temporary draft file under the system temp directory.
- Temporary draft filenames use a fixed prefix plus a random suffix.
- The draft extension uses `createExtension` if configured.
- Otherwise it falls back to the first writable pattern in that group.
- The draft is opened with `file_action.Edit`, and MyBuddy waits for that editor process to exit.
- If the draft file was not changed, it is discarded.
- If the draft file was changed, it is moved into the target group directory and renamed from content.
- Group context menu can also create a note directly from clipboard text.
- Clipboard-created notes use the same final filename extraction and conflict handling rules.
- `text` groups do not support creating new notes from the UI.

Final filename rules:

- For `.md` files:
  - If the file starts with TOML front matter and contains `title`, use that `title`.
  - Otherwise use the first non-empty content line.
  - If the first content line starts with Markdown heading markers such as `# ` or `## `, strip those markers first.
- For `.txt` files:
  - Use the first line.
- The chosen name is truncated to `64` characters.
- Invalid filename characters are replaced with `_`.
- If the resulting name already exists, a numeric suffix is appended.

## Context Menus

Group header right-click menu:

- `新建笔记` for directory groups only
- `从剪贴板新建` for directory groups only
- `刷新`
- group actions from `groupActions`

File item right-click menu:

- default file action
- file actions from `fileActions`

## Command Template Variables

Supported placeholders:

- `{file}`: absolute file path
- `{dir}`: note file directory
- `{name}`: filename with extension
- `{stem}`: filename without extension
- `{group}`: group id
- `{group_title}`: group title
- `{group_dir}`: group directory; for `text` groups this is the source file's parent directory
- `{line}`: line text for `text` items, otherwise empty
- `{line_number}`: 1-based line number for `text` items, otherwise `0`

## Parsing Rules

- Unknown sections should be ignored.
- Unknown keys should be ignored.
- Missing group `path` makes the group invalid.
- Missing action `command` makes the action invalid.
- Invalid action ids or wrong target types referenced by groups should be skipped.

## Current Implementation Scope

- One flat grouped list in the main window
- Collapsible group headers
- Add button on each group header
- Click file item runs `defaultFileAction`
- Right-click group header shows group menu
- Right-click file item shows file menu
