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
- `state.dat` is stored in `%AppData%\MyBuddy\`.
- `config.ini` is for stable settings.
- `state.dat` is for runtime state only.
- Start from [config.ini.example](/C:/~/%5CProjects%5CMyBuddy%5Cconfig.ini.example).

## Notes

- Notes are configured from `config.ini`.
- Each note group maps to a directory and is shown in one flat grouped list.
- Group headers can be collapsed and include a `+` button for creating a new note.
- File actions and group actions are configured as command lines in `config.ini`.
- The list shows inline status rows for empty groups, missing directories, and missing note-group configuration.
- Notes config reference: [docs/notes-config.md](/C:/~\Projects\MyBuddy\docs\notes-config.md)

## License

MIT
