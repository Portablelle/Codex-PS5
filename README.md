<p align="center">
  <img src="sce_sys/icon0.png" alt="Codex PS5 icon" width="128">
</p>

# Codex PS5

**Talk to Codex from your couch. Explore your PS5 files, dictate a request,
and review actions with your controller.**

Codex PS5 is a native homebrew app for a jailbroken PlayStation 5. The Codex
engine and file tools run on the console; responses use your online ChatGPT
account. Voice transcription runs locally, using Whisper and the DualSense
microphone.

[Download a release](https://github.com/Portablelle/Codex-PS5/releases) ·
[Installation & updates](INSTALL.md) · [What's new](CHANGELOG.md) ·
[Technical details](docs/REFERENCE.md)

![Codex PS5 conversation view](docs/images/conversation.png)
*Captured from the app's host renderer with a simulated conversation; these are
interface previews, not photographs or console captures. Storage values are sample data.*

## What you can do

- **Ask about your files.** Check storage, browse folders, inspect text, and ask
  Codex to help organize files with the bundled local tools.
- **Dictate with your controller.** Press △ to record and again to transcribe.
  Review the text, then press Options to send it.
- **Review actions on your TV.** Commands and file changes that need your
  approval appear with their details and a plain-language summary such as
  "Moving a file", "Editing files" or "Requesting network access". Decline is
  selected by default, and each choice stays in the conversation as a line
  such as "Allowed: Moving a file".
- **Use English or French.** The app follows the console language, with QWERTY
  in English and AZERTY in French. On a French console, R1 switches the interface
  language while idle without changing your conversation.
- **Sign in with your phone.** Press L1 and confirm the code shown on the TV.

<details>
<summary>More interface screenshots</summary>

### Controller keyboard

![Controller keyboard](docs/images/keyboard.png)

### Review an action

![Command approval dialog](docs/images/approval.png)

These previews use the same renderer as the native app, with simulated session
state. The approval example shows a file move that requires confirmation.
</details>

## Before installing

You need a jailbroken PS5 with native homebrew support through
[kstuff / ShadowMount](https://github.com/PSBrew/ShadowMountPlus), an ELF loader
(such as Relapse, on port 9021), and a computer or phone for transferring files.
The validation setup is **firmware 13.00 with Relapse/kstuff/ShadowMount**;
other firmware and tool combinations are not confirmed for this app.

Codex replies need an Internet connection and a ChatGPT account with Codex
access. This is an early prototype, independent of OpenAI and Sony.

## Install with PS5 Upload

Download these two files from the same
[release](https://github.com/Portablelle/Codex-PS5/releases):

| File | What it contains |
| --- | --- |
| `codex-ps5-0.0.4-Folder.zip` | Native app, runtime modules, and the complete Whisper Small Q5_1 model |
| `assistant-service.elf` | Codex engine and local file tools |

1. Extract the Folder ZIP. Using [PS5 Upload](https://github.com/phantomptr/ps5upload)
   or FTP, transfer **`PPSA99105`** into **`/data/homebrew`**.
2. Create **`/data/codex-ps5/payloads/assistant-service`** and upload
   **`assistant-service.elf`** there. Also create **`/data/ps5-ai-cli`** for the
   loader's legacy lock file.
3. Keep your jailbreak, kstuff, ShadowMount, and ELF loader active. Send
   **the same `assistant-service.elf`** with PS5 Upload's payload sender to the
   console's ELF loader on port **9021**.
4. Once ShadowMount has registered the title, open **Codex PS5** from your
   library. Press **L1** to sign in.

The resulting paths must include:

```text
/data/homebrew/PPSA99105/eboot.bin
/data/homebrew/PPSA99105/assets/ggml-small-q5_1.bin
/data/codex-ps5/payloads/assistant-service/assistant-service.elf
```

Check that the provisional title ID **PPSA99105** is unused before your first
install. Upload the complete folder, including `sce_sys`, `sce_module`, and
`assets`. The service must be loaded again after a console reboot.

The ZIP is a **ShadowMount Folder**, and the ELF is a **payload**. Use PS5
Upload's transfer and payload features; its PKG installer is for actual `.pkg`
files. See [the full guide](INSTALL.md) for FTP, Payload Manager, and troubleshooting.

## Update an existing installation

1. Close Codex PS5 and any experimental companion, then reboot the PS5 to stop
   the old service. Reactivate your jailbreak, kstuff, ShadowMount, and ELF loader.
2. Back up the existing app folder and service ELF. Replace the **complete
   `PPSA99105` folder** and **`assistant-service.elf`** with files from the same
   release. Let the transfer finish before launching.
3. Send the new service ELF and open the app again.

Keep **`/data/codex-ps5/home`** and **`/data/codex-ps5/workspace`**: they contain
sign-in data and your files. Do not delete `/data/codex-ps5` to update.
Details and rollback steps are in [INSTALL.md](INSTALL.md#updates).

## Controls

| Button | Action |
| --- | --- |
| △ | Start / finish dictation |
| □ | Open / close the keyboard |
| ✕ | Select / confirm |
| Options | Send the prompt |
| L1 | Sign in to ChatGPT |
| R1 | Switch French / English on a French PS5, while idle |
| ○ | Cancel dictation, interrupt a response, or decline |
| ↑ / ↓ | Scroll the conversation while the keyboard is closed |
| Touchpad click | Open the quit confirmation |

## Prototype limitations

- The last conversation resumes when the app reopens or reconnects to the
  engine; enter `/new` and press Options to start a new one. Only messages are
  restored, and disconnects never resend prompts automatically.
- Transcription runs on the PS5 CPU and can take time. Always review the text.
  Audio stays on the console; submitted prompts go to the online Codex service.
- The engine runs with the payload's permissions, without a system sandbox.
  Recognized simple inspection commands are accepted automatically; other
  commands and file changes ask for approval. Trust the homebrew running on
  your console: local service sockets are unauthenticated.
- The in-game companion is an **experimental development component**. Overlay,
  game capture, microphone, transcription, and spoken playback are not connected
  to it. It is not part of the public installation package.

## Build from source

```sh
bash tools/test.sh
PS5_BUILD_HOST=build-host bash tools/build-remote.sh
PS5_BUILD_HOST=build-host bash tools/build-backend-remote.sh
python3 tools/fetch-model.py
python3 tools/package.py
```

Docker builds run on a remote Linux host. Public release files are assembled
under **`dist/release/`**, including `INSTALL.md`, licenses, release metadata,
and `SHA256SUMS`. The legacy combined ZIP and `dist/bundle` remain available for
Botty integrations. See [the technical reference](docs/REFERENCE.md#building)
for prerequisites and [release preparation](docs/RELEASING.md) for maintainers.

## Credits

Built on [ps5-ai-cli](https://github.com/saawant12/ps5-ai-cli),
[OpenNOW PS5](https://github.com/Portablelle/opennow-ps5), the Botty+ renderer,
and [whisper.cpp](https://github.com/ggml-org/whisper.cpp).
See [THIRD_PARTY.md](THIRD_PARTY.md) and [LICENSE](LICENSE) for attribution and licenses.
