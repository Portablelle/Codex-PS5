# Codex PS5

Native PS5 client for the Codex app-server (`src/`), the engine service it talks
to (`backend/`), and an experimental in-game voice companion (`src/ingame.*`,
`backend/ingame-service.cpp`, see `INGAME.md`). `README.md` is the public overview and
`docs/REFERENCE.md` describes the detailed user-facing behavior; keep both, `INSTALL.md`
and `INGAME.md` in sync with the code you change.

## Build and test

- `bash tools/test.sh` (or `make test`) is the whole host test suite: it compiles
  the C++ unit tests with `${CXX:-clang++}`, builds `build/ingame-service`, and runs
  `tests/resident_service_test.py` (real loopback sockets, fake engines, no network).
  Run it before every push. CI runs it on Linux and macOS (`.github/workflows/tests.yml`).
- `tests/resident_service_test.py` needs `build/ingame-service`, so run `tools/test.sh`
  once before running it on its own.
- `bash tools/preview.sh` renders the UI to `build/preview.png` on the host (needs Pillow).
- The `ps5-build` CI job compile-checks with the PS5 SDK and `-Werror`, on a self-hosted
  runner: `backend/assistant-service.c` and the in-game service
  (`tools/ps5-compile-check.sh`), and every native app source in `src/`
  (`tools/ps5-native-check.sh`). It links nothing: no package, whisper.cpp or Codex engine.
- Not available in CI or locally: the full PS5 builds
  (`tools/build-*-remote.sh`), Codex itself
  (`tests/app_server_handshake.py`) and the console. Say so when a change depends on them.

## Platforms

Code runs on three targets: the PS5 (FreeBSD-based, `prospero-clang -Werror`),
macOS (the maintainer's workstation) and Linux (CI). Socket, `poll()` and signal
semantics differ between them (e.g. `POLLHUP` on a closed socketpair is reported on
Linux but not always on macOS). Do not assume a behavior seen on one host holds on the
others, and keep code warning-free under `-Wall -Wextra`.

## Conventions

- Source strings, docs, commit messages and PR text are in English.
- UI strings go through `assistant::language::tr()`; every new user-visible string
  needs a French entry in `src/language.hpp` (match the style of existing entries)
  and an assertion in `tests/language_test.cpp`.
- Match the surrounding code style: compact C++, few comments, no new dependencies.

## Invariants to preserve

- Approvals: only the recognized read-only commands listed in `docs/REFERENCE.md` are
  auto-accepted, never for the whole session; everything else asks the user, with
  **Decline** as the default choice.
- Gameplay mode (`src/ingame.cpp`) uses `read-only` / `never` and declines every
  command or file-change approval. A failed, empty or over-long answer is never spoken.
- Every in-game platform callback carries a request ID; stale IDs or wrong-phase
  callbacks are ignored.
- Prompts are never automatically resent after a disconnect.
- Loopback endpoints are unauthenticated; do not widen what they accept without saying so.

## Tests

- A bug fix comes with a regression test that fails without the fix. Prove it: run
  the test against the old code (`git checkout <base> -- <file>`), then restore.
- Avoid timing-dependent tests (`sleep`, races between processes); wait on an
  observable condition instead, and assert the test's own preconditions.
