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
[notes]
filePatterns=*.txt;*.md
maxItems=5
sortBy=mtime
sortOrder=desc
defaultGroupExpanded=1

[action.edit]
title=编辑
command="C:\Program Files\Notepad++\notepad++.exe" "{file}"

[action.reveal]
title=打开目录
command=explorer.exe /select,"{file}"

[action.terminal]
title=终端打开本组
command=wt.exe -d "{group_dir}"

[note_group.work]
title=工作
path=D:\Notes\Work
expanded=1
filePatterns=*.md
maxItems=8
sortBy=name
sortOrder=asc
defaultFileAction=edit
fileActions=edit;reveal
groupActions=terminal

[note_group.personal]
title=个人
path=D:\Notes\Personal
expanded=0
defaultFileAction=edit
fileActions=edit;reveal
groupActions=
```

## Global Section

Section name: `[notes]`

Supported keys:

- `filePatterns`
  - Semicolon-separated patterns.
  - Default: `*.txt;*.md`
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

## Action Sections

Section format: `[action.<id>]`

Supported keys:

- `title`
  - User-facing menu text.
- `command`
  - Full command template.
  - Executed directly via `CreateProcessW`.
  - If shell behavior is needed, explicitly configure `cmd.exe /c ...`.

Example:

```ini
[action.edit]
title=编辑
command="C:\Program Files\Notepad++\notepad++.exe" "{file}"
```

## Group Sections

Section format: `[note_group.<id>]`

Required keys:

- `title`
- `path`

Optional keys:

- `expanded`
  - `1` or `0`
  - If missing, inherits `defaultGroupExpanded`
- `filePatterns`
  - If missing, inherits global `filePatterns`
- `maxItems`
  - If missing, inherits global `maxItems`
- `sortBy`
  - If missing, inherits global `sortBy`
- `sortOrder`
  - If missing, inherits global `sortOrder`
- `defaultFileAction`
  - Action id used for double-click or primary open behavior
- `fileActions`
  - Semicolon-separated action ids shown in file item context menu
- `groupActions`
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

- The add button on a group header creates one new file in that group directory.
- The new file extension should use the first writable pattern in that group.
- Suggested default naming format:
  - `yyyyMMdd-HHmmss.txt`
  - `yyyyMMdd-HHmmss.md`
- After creation, run `defaultFileAction` if configured.

## Context Menus

Group header right-click menu:

- `新建笔记`
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
- Invalid action ids referenced by groups should be skipped.

## Current Implementation Scope

- One flat grouped list in the main window
- Collapsible group headers
- Add button on each group header
- Click file item runs `defaultFileAction`
- Right-click group header shows group menu
- Right-click file item shows file menu
