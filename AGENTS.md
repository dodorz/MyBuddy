# AGENTS

## Versioning

- `MyBuddy` uses a three-part version number:`major.minor.patch`
- `./src/version.h` is the only source of truth for the current version.
- If `major` increases by `1`, reset both `minor` and `patch` to `0`.
- If `minor` increases by `1`, reset `patch` to `0`.
- GitHub Actions supports manual builds via `workflow_dispatch`.
- GitHub Actions accepts version tags with or without a leading `v`, normalizes them to `v<version>`, and auto-builds pushed version tags when version tag changes compared with the previous one.
- Release assets are packaged as `MyBuddy-v$version-windows-x64.zip`.
