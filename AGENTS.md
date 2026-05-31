# AGENTS

## Versioning

- `MyBuddy` uses a three-part version number:`major.minor.patch`
- `./src/version.h` is the only source of truth for the current version.
- If `major` increases by `1`, reset both `minor` and `patch` to `0`.
- If `minor` increases by `1`, reset `patch` to `0`.
- GitHub Actions supports manual builds via `workflow_dispatch`.
- GitHub Actions accepts version tags with or without a leading `v`, normalizes them to `v<version>`, and auto-builds pushed version tags when version tag changes compared with the previous one.
- Pushed version tags automatically create or update a GitHub Release.
- Workflow artifacts include both `MyBuddy-v$version-windows-x64-Debug.zip` and `MyBuddy-v$version-windows-x64-Release.zip`.
- GitHub Releases automatically attach only `MyBuddy-v$version-windows-x64-Release.zip`.
