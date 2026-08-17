# MyBuddy

Minimal native Win32 desktop tool with edge docking and auto-hide behavior.

## Build

```powershell
cmake --preset vs-x64
cmake --build --preset vs-x64-debug
cmake --build --preset vs-x64-release
```

## Configuration

- `config.ini` is read from the program directory first, then `%AppData%\MyBuddy\`.
- `config.ini` must be saved as UTF-8 without BOM.
- `state.dat` is stored in `%AppData%\MyBuddy\`.
- The AppData directory is always `%AppData%\MyBuddy\`.
- `config.ini` is for stable settings.
- `state.dat` is for runtime state only.
- Start from [config.ini.example](/C:/~/%5CProjects%5CMyBuddy%5Cconfig.ini.example).

## Notes

- Notes are configured from `config.ini`.
- Each note group maps to a directory and is shown in one flat grouped list.
- Group headers can be collapsed and include a `+` button for creating a new note.
- Drag directory-group file items onto a directory-group header, subdirectory header, or file item to move the file into that target folder. Drops within the current folder are ignored; existing destination filenames are never overwritten.
- Drag `textgroup` or `todogroup` line items onto another line in the same group to reorder source-file lines. The blue marker indicates whether the line will be inserted before or after the target.
- File actions and group actions are configured as command lines in `config.ini`.
- The list shows inline status rows for empty groups, missing directories, and missing note-group configuration.
- Notes config reference: [docs/notes-config.md](/C:/~\Projects\MyBuddy\docs\notes-config.md)

## License

MIT
