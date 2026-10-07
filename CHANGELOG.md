# Changelog

## 0.0.4 — prepared prerelease

Changes since the 0.0.3 prototype:

### Conversations

- Resume the last conversation after closing the app or reconnecting, restoring
  a bounded history of up to 20 turns, 64 messages, or 1 MiB.
- Keep the remembered conversation when resuming fails, show the engine's reason,
  and fall back to a new conversation without automatically resending a prompt.
- Start a fresh conversation between turns by entering `/new` and pressing Options.
- Retry a failed conversation start by sending a prompt again instead of leaving
  the interface unable to send.

### Approvals

- Summarize command and file-change approval requests in plain language while
  retaining their technical details and keeping Decline selected by default.
- Keep approval details for running items until completion, with oldest-first
  eviction when the cache limit is reached.
- Record each approval decision in the conversation as an allowed or declined
  action, translated into the interface language.

The downloads remain a complete ShadowMount Folder ZIP with Whisper Small Q5_1
and a separate `assistant-service.elf`. The experimental in-game companion is
not included; see [INGAME.md](INGAME.md).

Release preparation does not claim a new end-to-end console test. Prompts
interrupted by a disconnect are never automatically resent.

## 0.0.3 — prepared prerelease

Changes since the 0.0.2 prototype:

### Interface and everyday use

- Redesigned native chat interface with controller button symbols.
- PS5 system language detection: French on a French console, English otherwise.
  French-only R1 switching stays local to the interface and preserves the account
  and conversation.
- AZERTY controller keyboard in French and QWERTY in English.
- Quit confirmation through the touchpad or `/quit`, with Cancel selected by default.

### Voice and approvals

- Multilingual Whisper **Small Q5_1**, replacing the earlier Base model, with
  automatic language detection, six CPU threads, and greedy decoding.
- 1 GiB native heap and a free-memory check before loading the model.
- Automatic approval of recognized simple read-only commands, including their
  supported bundled-shell wrappers. Other commands and file changes still need
  explicit approval with Decline selected by default.
- Fixes for shutdown stalls, deferred interrupts, and sign-in state handling.

### Distribution

- Complete ShadowMount **Folder ZIP** with the voice model at release time,
  plus a standalone `assistant-service.elf` for existing payload senders.
- Installation and update guide for PS5 Upload / FTP, preserving credentials
  and workspace files, with rollback instructions.
- Public README with interface previews; technical details moved to a reference.
- SHA-256 checksums, service build metadata, and redistribution notices.
- Packaging rejects a stale backend, a mismatched title version/model, and
  the AudioIn link stub; regenerating packages removes obsolete staging files.

### Experimental development work

- In-game voice companion session core, resident ELF service, and host probe.
- Backend support for two independent client sessions.
- Host tests on Linux/macOS and PS5 SDK compile checks for backend and native sources.

The experimental companion is not included in the public release downloads.
Its overlay, capture, microphone, transcription, and spoken playback are not
connected yet; see [INGAME.md](INGAME.md).

The validation setup for the project is firmware 13.00 with
Relapse/kstuff/ShadowMount. This release preparation validates builds and host
tests; it does not claim a new end-to-end console test of every addition.

## 0.0.2 — prototype

- Portal update control with verified downloads and preservation of user data.
- Local status and idle shutdown endpoint for the engine service.

## 0.0.1 — initial prototype

- Native controller interface, ChatGPT sign-in, and on-console Codex app-server.
- Local dictation and bundled file utilities.
