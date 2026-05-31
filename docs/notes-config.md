# Notes Configuration

This document defines the `config.ini` format for the notes feature.

## Overview

- Each note group maps to one directory.
- Each text or markdown file in that directory is one note item.
- Groups are shown together in one list.
- Group headers are collapsible.
- The add button belongs to the group header.
- File actions and group actions are both defined in config.

## Example

```ini
[notes_default]
filePatterns=*.txt;*.md
createExtension=.md
maxItems=5
sortBy=mtime
sortOrder=desc
defaultGroupExpanded=1
showExtensions=0
defaultFileAction=Edit
fileActions=Edit;reveal
groupActions=terminal

[file_action.Edit]
title=编辑
command="C:\Program Files\Notepad++\notepad++.exe" "{file}"

[file_action.reveal]
title=打开目录
command=explorer.exe /select,"{file}"

[dir_action.terminal]
title=终端打开本组
command=wt.exe -d "{group_dir}"

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

- `filePatterns`
  - Semicolon-separated patterns.
  - Default: `*.txt;*.md`
- `createExtension`
  - Default extension used when creating a new note.
  - Examples: `.txt`, `.md`
  - If missing or empty, falls back to the first writable extension from `filePatterns`
- `maxItems`
  - Max visible note items per group.
  - Default: `5`
- `sortBy`
  - Supported: `name`, `ctime`, `mtime`
  - Default: `mtime`
- `sortOrder`
  - Supported: `asc`, `desc`
  - Default: `desc`
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

Optional keys:

- `title`
  - If missing, defaults to the group id from `[note_group.<id>]`

- `expanded`
  - `1` or `0`
  - If missing, inherits `defaultGroupExpanded`
- `showExtensions`
  - `1` or `0`
  - If missing, inherits default `showExtensions`
- `filePatterns`
  - If missing, inherits default `filePatterns`
- `createExtension`
  - If missing, inherits default `createExtension`
- `maxItems`
  - If missing, inherits default `maxItems`
- `sortBy`
  - If missing, inherits default `sortBy`
- `sortOrder`
  - If missing, inherits default `sortOrder`
- `defaultFileAction`
  - Must reference a `file_action`
  - If missing, inherits default `defaultFileAction`
  - Action id used for double-click or primary open behavior
- `fileActions`
  - Must reference `file_action` ids
  - If missing, inherits default `fileActions`
  - Semicolon-separated action ids shown in file item context menu
- `groupActions`
  - Must reference `dir_action` ids
  - If missing, inherits default `groupActions`
  - Semicolon-separated action ids shown in group header context menu

## Sorting Rules

- Files are filtered first.
- Files are sorted second.
- `maxItems` is applied last.
- If `sortBy=name`, secondary sort is `mtime desc`.
- If `sortBy=ctime`, secondary sort is `name asc`.
- If `sortBy=mtime`, secondary sort is `name asc`.

## File Matching

- `filePatterns` accepts shell-style patterns such as `*.txt` and `*.md`.
- Matching is case-insensitive.
- Only files are included, not subdirectories.
- First implementation should enumerate one directory level only.

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

- `新建笔记`
- `从剪贴板新建`
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
- `{group_dir}`: group root directory

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
