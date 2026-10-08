# Preparing a public release

The public install format is a complete ShadowMount Folder ZIP plus a separate
ELF service. PS5 Upload supports folder/archive transfers and payload sending;
FTP and existing ELF senders remain usable. The native platform's optional
`.ffpkg` output is a filesystem image, not an installable `.pkg`.

1. Update `VERSION`, `sce_sys/param.json` (`contentVersion`), the `clientInfo`
   version in `src/protocol.cpp` (`tools/test.sh` checks it against `VERSION`),
   the download names in `README.md` / `INSTALL.md`, and `CHANGELOG.md` together.
   `src` is part of the backend build identity, so make these changes before the
   release builds.
2. Run `bash tools/test.sh` before pushing. Build the current native app and
   backend on the remote Linux Docker host, then fetch the checksum-pinned model:

   ```sh
   PS5_BUILD_HOST=build-host bash tools/build-remote.sh
   PS5_BUILD_HOST=build-host bash tools/build-backend-remote.sh
   python3 tools/fetch-model.py
   python3 tools/package.py
   ```

   Use `PS5_BUILD_DIRECTORY` to isolate a release build from other work on that
   host. That directory mirrors the checkout, so it must be dedicated to these
   builds: the scripts refuse a non-empty directory that is not already one.
   The first Codex build needs several GB of RAM and disk space.
   Rebuild the backend if `backend`, `src`, or `tools` changes: the package checks
   its build identity against those inputs.
3. Commit and push the release changes, then open the PR. Create a **draft
   prerelease**, targeting that exact commit, with every file in `dist/release`
   attached. Use the current changelog entry as the release notes and add the
   installation link and validation results. Do not attach the intermediate
   `dist/PPSA99105.zip`: it does not contain the Whisper model.
4. Review and merge the PR before publishing the draft. Keep the Git tag on the
   commit used to build the assets; if review changes any build inputs, rebuild
   and replace the draft assets. GitHub's source archives must correspond to
   the binaries. Record any console testing separately from host/build checks.

`dist/release` contains the complete title ZIP, engine ELF, install guide,
licenses/notices, `release.json`, and `SHA256SUMS`. The legacy combined ZIP
(`dist/codex-ps5-<version>.zip`) and `dist/bundle` remain for the Botty exporter;
these are not the primary public downloads.

## README images

The images under `docs/images` are captures from the native renderer's host
preview, with simulated data. Regenerate on macOS with Pillow installed:

```sh
bash tools/preview.sh
mkdir -p docs/images
cp build/preview.png docs/images/conversation.png
PREVIEW_KEYBOARD=1 bash tools/preview.sh
cp build/preview.png docs/images/keyboard.png
PREVIEW_APPROVAL=1 bash tools/preview.sh
cp build/preview.png docs/images/approval.png
```

Keep the simulated-data caption. Console screenshots can replace these when
available, after excluding account details, credentials, and private prompts.

## Automated Portal packages

The `Portal release` workflow builds every `main` push and publishes a prerelease
tagged `portal-<40-character commit SHA>` with the single `codex-portal.tar.gz`
asset. Both build and publication run on dedie's isolated `codex-ps5-ci` slot.
Same-repository pull requests exercise the same package build with read-only
repository permissions; they cannot publish. Fork PRs do not enter this runner.
Manual workflow runs are allowed only on `main`.

The runner image must supply the pinned backend PS5 SDK at `/opt/ps5-payload-sdk`,
LLVM 18 and 19, Rust 1.95 with `rust-src` and the FreeBSD target, and the Rust
standard-library patch from `vendor/ps5-ai-cli/tools/prepare-rust-std.py` applied
at image build time. Native dependencies are downloaded and hash-verified by
the existing native bootstrap. The backend uses `prepare-backend.py --direct`
to adapt absolute Docker paths only in its ignored staging directory. Jobs need
no Docker socket, SSH keys, sudo privileges or deployment credentials. Backend
builds need more writable capacity than the small Portal test slot; provision
bounded scratch space for Cargo and the complete source/build tree.

`tools/package-portal.py` rejects uncommitted/untracked source changes and calls
the existing `package.py` validation before export. Its archive contains
`apps/codex/{manifest.json,chunks,LICENSE,NOTICE.md,codex-source.tar.gz}` and
`src/codex-payload.js`. The source archive is made with `git archive HEAD`, so
ignored private state, build output, dependency caches and generated vendor
checkouts never enter it; tracked public vendor source remains included.
The root `codex-release.json` has `schema: 1`, `commit`, `version` and a `sha256`
map for every package file except itself, using package-root-relative names.
The source tar is covered by this map, not the chunk manifest's `files` array.

Publication creates a draft, uploads the asset, then publishes the prerelease
without changing GitHub's latest stable release. Existing published assets must
match byte-for-byte on a rerun and are never overwritten. A failed publication
may leave a draft; a rerun uploads its missing asset or checks the existing asset
before publishing that draft. A mismatching existing asset fails closed and needs
manual investigation rather than replacement.
Re-running all build jobs succeeds against an existing asset only if the native
and backend outputs reproduce byte-for-byte; reproducible PS5 linking is not
assumed, so differences deliberately fail without modifying the release.
