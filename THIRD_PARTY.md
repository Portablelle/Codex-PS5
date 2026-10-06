# Third-party components

* **PS5 native app boilerplate** — BlackBearReloaded, GPL-3.0-or-later.
  Renderer, VideoOut platform and tooling retained with their attribution.
  Tooling snapshot from OpenNOW PS5 (Portablelle), with its revision in
  `upstream-lock.json`. The renderer and transport base comes from Botty+.
* **ps5-ai-cli** — saawant12 and contributors. Snapshot under `vendor/ps5-ai-cli`.
  See its GPL license and `LICENSES/` for Codex (Apache-2.0), Dash,
  sbase, the SDK, Mio and other transitive components. This project's
  adaptations are in `backend/` and `tools/prepare-backend.py`.
* **PS5 AudioIn research** — BlackBearReloaded, GPL-3.0. Reference for signatures
  and capture configuration:
  https://github.com/blackbearreloaded/ps5-audio-decoding-research,
  `examples/native-audio-poc/microphone/src/main.cpp`.
* **whisper.cpp / ggml** — ggml-org and contributors, MIT.
  Version 1.8.3; license in `licenses/Whisper-MIT.txt`.
  Multilingual Whisper `small` model quantized to Q5_1, with provenance and checksum in the lockfile.
* **JSON for Modern C++** — Niels Lohmann and contributors, MIT.
  Version 3.12.0, attribution and license included in `vendor/json.hpp`.
* **Manrope** — Mikhail Sharanda and contributors, SIL OFL 1.1.
  Font source under `vendor/manrope`, license under `licenses/`.

AudioIn stubs provide imports during linking. They do not reproduce the Sony
module and must not be distributed with the application; the console's AudioIn
module supplies the actual functions.
