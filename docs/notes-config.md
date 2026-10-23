# Notes Configuration

This document defines the `config.ini` format for the notes feature.

The config file must be saved as UTF-8 without BOM.

## Overview

- Each note group maps either to one directory or to one text file.
- Directory groups use files as items.
- `text` groups use non-empty lines in one text file as items.
- If a `text` group points to a Markdown file and that file starts with TOML/YAML front matter delimited by `+++` or `---`, the front matter block is ignored.
- Groups are shown together in one list.
- Group headers are collapsible.
- Directory-group file items can be dragged to a directory-group header, subdirectory header, or file item to move them into that folder. The source and destination may belong to different groups or to different direct subdirectories of one group. Drops into the current folder are ignored and MyBuddy never overwrites an existing destination file.
- `textgroup` and `todogroup` line items can be dragged onto another line in the same group to reorder their source-file lines. Drop above or below the target line to choose the insertion position.
- The add button belongs to the group header.
- File actions and group actions are both defined in config.

## App Section

Section name: `[app]`

Supported keys:

- `globalHotKey`
  - Legacy global hotkey used to show the window
  - Default: `Ctrl+Alt+B`
  - Same key syntax as `[hotkey.<id>]` `key`
  - If a `[hotkey.<id>]` binding uses the same key, that binding takes precedence and the legacy entry is skipped
- `defaultGroup`
  - Optional fallback group id for `group.addNote`, `group.addFromClipboard`, `group.action`, and `file.action` when `group=` is omitted
  - Default: empty

## Hotkey Sections

Section format: `[hotkey.<id>]`

Each section defines one global shortcut. The `<id>` is only used for diagnostics and uniqueness; it does not affect behavior.

Supported keys:

- `key`
  - Required
  - Hotkey text such as `Ctrl+Alt+N`, `Ctrl+Shift+1`, or `Alt+F6`
  - At least one modifier is required: `Ctrl`, `Alt`, `Shift`, or `Win`
  - Modifier aliases: `Control`, `Windows`, `Meta`
  - Named non-modifier keys: `F1`-`F24`, `Space`, `Tab`, `Enter`, `Esc`, `Backspace`, `Delete`, `Insert`, `Home`, `End`, `PageUp`, `PageDown`, `Up`, `Down`, `Left`, `Right`, `Minus`, `Equal`, `Comma`, `Period`, `Slash`, `Semicolon`, `Quote`, `Backslash`, `LBracket`, `RBracket`, `Backtick`
  - Single letters and digits are supported
  - The `+` key itself is not directly expressible because `+` separates parts; use `Shift+Equal` or another key
- `command`
  - Required
  - One of the commands listed below
- `group`
  - Group id, not group title
  - Used by group commands and `file.action` when using `item=`
- `action`
  - Action id from `[action.<id>]`
  - Required by `group.action`, `file.action`, and `current.action`
- `file`
  - Absolute file path for `file.action`
- `item`
  - Optional file name, stem, or full path inside `group=` for `file.action`
- `wait`
  - `1` or `0`
  - Default: `0`
  - When `1`, actions that support waiting use the blocking execution path and refresh the group afterward
  - Hotkey actions default to non-blocking execution so the main window does not freeze while an editor is open

Supported commands:

- `window.toggle`
  - Show and activate the main window
- `window.show`
  - Same as `window.toggle`
- `config.reload`
  - Reload `config.ini` and refresh notes
- `config.open`
  - Open `config.ini` with the configured `Edit` action
- `group.toggle`
  - Expand or collapse a group
- `group.addNote`
  - Create a new note in a directory group
- `group.addFromClipboard`
  - Create a new note from clipboard text in a directory group
- `group.action`
  - Run an action in group context
  - Directory actions receive `{group_dir}` etc.
  - For `textgroup` and `todogroup`, file-target actions receive the source file when it exists
- `file.action`
  - Run an action for one explicit file
  - Target is selected with `file=`, or with `group=` and `item=`
- `current.action`
  - Run an action for the current selected row
  - File row: file context
  - Group or subdirectory row: group context
  - If no row is selected, the command reports an error; it does not guess a target

Examples:

```ini
[app]
globalHotKey=Ctrl+Alt+B
defaultGroup=inbox

[hotkey.inboxNew]
key=Ctrl+Alt+N
command=group.addNote
group=inbox

[hotkey.inboxClipboard]
key=Ctrl+Alt+V
command=group.addFromClipboard
group=inbox

[hotkey.workTerminal]
key=Ctrl+Alt+T
command=group.action
group=work
action=Terminal

[hotkey.dailyEdit]
key=Ctrl+Alt+D
command=file.action
file=D:\Notes\Work\daily.md
action=Edit

[hotkey.revealCurrent]
key=Ctrl+Alt+R
command=current.action
action=Reveal
```

Notes:

- Hotkeys are system-wide while MyBuddy is running.
- Duplicate hotkeys and keys already owned by another program are reported through the tray notification area; other bindings still register.
- `MOD_NOREPEAT` is used so holding a hotkey does not repeatedly trigger the command.
- `Ctrl+Alt` is equivalent to AltGr on some keyboard layouts; prefer `Ctrl+Shift`, `Ctrl+Alt+Shift`, or `Win+Shift` combinations if typing special characters is affected.

## Toolbar Section

Optional section name: `[toolbar]`

Supported keys:

- `buttons`
  - Semicolon-separated toolbar button ids
  - Buttons are shown left-to-right in this order
  - If omitted, all `[toolbar_button.*]` sections are auto-discovered

Toolbar button section format: `[toolbar_button.<id>]`

Supported keys:

- `title`
  - Tooltip text
  - If missing, defaults to the button id
- `icon`
  - Supported: `touch`, `proxy`, `terminal`, `folder`, `refresh`, `edit`, `copy`
  - Built-in toolbar glyph name, not a file path
- `scope`
  - Supported: `global`, `group`, `file`
  - Default: `global`
- `command`
  - Full command template
  - Uses the same placeholders as `file_action` commands
  - `file` scope waits for the command to exit, then refreshes that group

## Example

```ini
[app]
globalHotKey=Ctrl+Alt+B

[notes_default]
maxItems=5
defaultGroupExpanded=1
defaultFileAction=Edit
fileActions=Edit;reveal
groupActions=terminal

[notes_dir_default]
filePatterns=*.txt;*.md
createExtension=.md
sortBy=mtime
sortOrder=desc
showExtensions=0

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

[textgroup.todo]
title=待办
path=D:\Notes\todo.txt
sortBy=line
defaultFileAction=Edit
fileActions=reveal
groupActions=terminal

[dirgroup.work]
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

[dirgroup.personal]
title=个人
path=D:\Notes\Personal
expanded=0
defaultFileAction=Edit
fileActions=Edit;reveal
groupActions=
```

## Drag and Drop

- Dragging begins after the pointer moves beyond the system drag threshold, so normal clicks still open items or toggle checkboxes.
- For directory groups, valid drop targets are directory-group headers, shown subdirectory headers, and visible directory-group file items. The target is the folder represented by that row; the moved filename is preserved.
- For text and todo groups, a drop is valid only on a different visible line in the same group. The source file's encoding, byte-order mark, and newline convention are preserved when its lines are rewritten.
- Line order is the physical order in the source file. Set `sortBy=line` and `sortOrder=asc` if the displayed order should directly match drag-and-drop order.

## Default Section

Section name: `[notes_default]`

Supported keys:

- `maxItems`
  - Max visible note items per group.
  - `0` means unlimited
  - Default: `5`
- `maxPinned`
  - Max number of directory-group Markdown files that are actually treated as pinned
  - `0` disables pinned handling
  - Default: `3`
- `defaultGroupExpanded`
  - `1` or `0`
  - Default: `1`
- `defaultFileAction`
  - Default file-item action inherited by groups
  - Must reference a `file_action`
- `deleteCommand`
  - Optional command used by the built-in Delete menu item
  - Uses the same placeholders as `file_action` commands
  - If missing, Delete moves the target file to Recycle Bin
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

These sections inherit from `[notes_default]`, then each `[dirgroup.<id>]` or `[textgroup.<id>]` section can override again.

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
- `maxPinned`
  - Max number of Markdown files treated as pinned in each directory group
  - `0` disables pinned handling
  - Default: inherits `[notes_default]`, otherwise `3`
- `sortBy`
  - Supported: `name`, `ctime`, `mtime`
  - Default: `mtime`
- `sortOrder`
  - Supported: `asc`, `desc`
  - Default: `desc`
- `showExtensions`
  - Whether `dir`-group file items show filename extensions by default
  - Supported: `1`, `0`
  - Default: `0`
- `defaultFileAction`
  - Default file-item action inherited by dir groups
- `deleteCommand`
  - Optional command used by the built-in Delete menu item for dir-group file items
  - If missing, Delete moves the file to Recycle Bin
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
- `defaultFileAction`
  - Default file-item action inherited by text groups
- `deleteCommand`
  - Optional command used by the built-in Delete menu item for text-group source files
  - If missing, Delete moves the source file to Recycle Bin
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
- `showConsole`
  - Whether to show the console window for console applications.
  - `1` = show console window, `0` = hide console window.
  - Default: `-1` (auto-detect; console apps hide the window, GUI apps show it).

Example:

```ini
[file_action.Edit]
title=编辑
command="C:\Program Files\Notepad++\notepad++.exe" "{file}"
```

## Group Sections

Section formats:

- `[dirgroup.<id>]`
- `[textgroup.<id>]`

Legacy `[note_group.<id>]` is still accepted for compatibility.

Required keys:

- `path`
  - `dirgroup`: one directory path
  - `textgroup`: one text file path

Optional keys:

- `title`
  - If missing, defaults to the group id from the section name

- `expanded`
  - `1` or `0`
  - If missing, inherits `defaultGroupExpanded`
- `showExtensions`
  - `1` or `0`
  - Applies to `dir` groups only
  - If missing, inherits typed default `showExtensions`
- `filePatterns`
  - If missing, inherits typed default `filePatterns`
- `createExtension`
  - If missing, inherits typed default `createExtension`
- `maxItems`
  - If missing, inherits typed default `maxItems`
  - `0` means unlimited
- `maxPinned`
  - Applies to `dir` groups only
  - If missing, inherits typed default `maxPinned`
  - `0` disables pinned handling
- `sortBy`
  - If missing, inherits typed default `sortBy`
- `sortOrder`
  - If missing, inherits typed default `sortOrder`
- `defaultFileAction`
  - Must reference a `file_action`
  - If missing, inherits typed default `defaultFileAction`
  - Action id used for double-click or primary open behavior
- `deleteCommand`
  - If missing, inherits typed default `deleteCommand`
  - Uses the same placeholders as `file_action` commands
  - If still missing, Delete moves the target file to Recycle Bin
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
- Effective pinned Markdown files are moved to the top.
- `maxItems` is applied last.
- `maxItems=0` disables item count truncation.
- `maxPinned=0` disables pinned handling.
- If `sortBy=line`, items keep source line order.
- If `sortBy=name`, secondary sort is `mtime desc`.
- If `sortBy=ctime`, secondary sort is `name asc`.
- If `sortBy=mtime`, secondary sort is `name asc`.

## Pinned Markdown Notes

- Pinned state is read from Markdown front matter delimited by `+++` or `---`.
- The front matter key is `pinned`.
- Truthy values are `true`, `yes`, `on`, and `1`, case-insensitive.
- After normal sorting, the first `maxPinned` pinned files are moved to the top.
- Additional pinned files are not moved and keep their normal sorted position.
- Among pinned files and among non-pinned files, the configured sort order is preserved.
- `maxItems` is applied after pinned files are moved, so pinned files count toward the visible limit.

Example:

```markdown
+++
title = "Daily note"
pinned = true
+++

# Daily note
```

## File Matching

- `filePatterns` accepts shell-style patterns such as `*.txt` and `*.md`.
- Matching is case-insensitive.
- Only files are included, not subdirectories.
- First implementation should enumerate one directory level only.
- `text` groups do not use `filePatterns`.
- For Markdown source files, leading TOML/YAML front matter delimited by `+++` or `---` is skipped before line items are generated and before title/pinned metadata is read.

## Add Note Behavior

- The add button on a group header first creates one temporary draft file under the system temp directory.
- Temporary draft filenames use a fixed prefix plus a random suffix.
- The draft extension uses `createExtension` if configured.
- Otherwise it falls back to the first writable pattern in that group.
- The draft is opened with `file_action.Edit`, and MyBuddy does not wait for that editor process to exit.
- The main window stays fully interactive while the editor is open, and multiple editors can be open at the same time.
- When the editor process exits, MyBuddy inspects the draft and finalizes it.
- If the draft file was not changed, it is discarded.
- If the draft file was changed, it is moved into the target group directory and renamed from content.
- Group context menu can also create a note directly from clipboard text.
- Clipboard-created notes use the same final filename extraction and conflict handling rules.
- `text` groups do not support creating new notes from the UI.

Final filename rules:

- For `.md` files:
  - If the file starts with TOML/YAML front matter delimited by `+++` or `---` and contains `title`, use that `title`.
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
- delete item

Text-group header right-click menu:

- default file action for the source file
- file actions from `fileActions`
- delete item

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
- `[hotkey.*]` sections bind system-wide shortcuts to window, group, file, and current-selection commands
