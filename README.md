# MyBuddy

Minimal native Win32 desktop tool with edge docking and auto-hide behavior.

## Build

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

## Configuration

- `config.ini` is read from the program directory first, then `%AppData%\MyBuddy\`.
- `state.dat` is stored in `%AppData%\MyBuddy\`.
- `config.ini` is for stable settings.
- `state.dat` is for runtime state only.

## License

MIT
