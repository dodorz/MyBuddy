# AGENTS

## Versioning

- `MyBuddy` uses a three-part version number:`major.minor.patch`
- `./src/version.h` is the only source of truth for the current version.
- If `major` increases by `1`, reset both `minor` and `patch` to `0`.
- If `minor` increases by `1`, reset `patch` to `0`.
- `MYBUDDY_VERSION_STRING` must match the other two defines.
- The release tag must equal `MYBUDDY_VERSION_STRING` exactly, with no leading `v`
  (for example `0.1.17`). Bump `src/version.h` and tag the same commit in one step.
- Older tags in this repository drift from `src/version.h` (`0.1.12` tagged
  version `0.1.10`, `0.1.13` tagged `0.1.11`, `0.1.15` tagged `0.1.16`), the
  `0.1.16` tag was never created, and some older releases use a `v` prefix
  (`v0.1.3`..`v0.1.5` next to `0.1.3`..`0.1.5`). Do not copy those tags as a
  reference; keep version and tag in sync instead.

## GitHub Actions

- Workflow: `.github/workflows/build.yml`, runs on `windows-2025-vs2026`.
- Builds both `Debug` and `Release` with `cmake -S . -B build\vs2026 -G "Visual Studio 18 2026" -A x64`.
- Supports manual builds via `workflow_dispatch`, with an optional `version` input.
- Triggered by pushed tags matching `v*` or `*.*.*`, and by every such push, not
  only when the version changes.
- Tag names are used verbatim as the GitHub Release tag (`tag_name`). There is no
  normalization: a `v` prefix is stripped only for the `$version` used in file
  names and the release title. `v0.1.4` and `0.1.4` would therefore be distinct
  releases.
- When `workflow_dispatch` gets no `version` input, the version is read from
  `src/version.h`.
- Pushed tags automatically create or update a GitHub Release.
- Workflow artifacts (uploaded under the artifact name `MyBuddy`):
  - `MyBuddy-v$version-windows-x64-Debug.zip` containing `MyBuddy-Debug.exe`
  - `MyBuddy-v$version-windows-x64.zip` containing `MyBuddy.exe`
- The GitHub Release attaches exactly one asset,
  `MyBuddy-v$version-windows-x64.zip` (containing `MyBuddy.exe`). The upload step
  names that file explicitly rather than using a wildcard, so the Debug zip and
  any file left over from an earlier version are never attached.
- Release notes are generated from `git log` since the previous tag.
