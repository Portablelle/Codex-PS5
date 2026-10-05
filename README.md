# Codex PS5 0.0.2 — native prototype

An assistant for files on a jailbroken PS5, with a native controller interface
and local dictation. This project is not an official OpenAI or Sony application.
The validation target is firmware 13.00 with Relapse/kstuff/ShadowMount.
ChatGPT sign-in, local commands and chat have been tested on this console.

## How it works

Two components run **on the PS5**:

* `assistant-service.elf`: the Codex 0.160.0 engine ported by
  [ps5-ai-cli](https://github.com/saawant12/ps5-ai-cli), adapted to expose
  `codex app-server` on a **127.0.0.2:49322** socket. Shell tools and file
  modifications run locally.
* `PPSA99105`: a native graphical application built with tools from
  [OpenNOW PS5](https://github.com/Portablelle/opennow-ps5) and the Botty+ renderer.
  It uses the app-server JSONL protocol without a browser.

The Botty portal can start the engine through the **Codex PS5** option once the
files have been installed over FTP. The engine can also be launched with the
Relapse ELF loader or Payload Manager. The portal verifies its checksum,
but does not automatically download updates for this application.

The PPSA99105 title ID is provisional: check that it is available before the
first installation. The build scripts do not install anything on the console.

Sign in to ChatGPT using a code displayed on the TV and confirmed on a phone.
Codex responses require Internet access and an account with Codex access.
The selected model is **GPT-6.1 SOL**, with **low** reasoning effort.
The application does not include an SSH connection.

## Language

At startup, the PS5 system language determines the Codex language: French
(France or Canada) on a French console, and English for all other languages
or if reading the setting fails. On a French console only, **R1** switches
between French and English while Codex and the microphone are idle.
The choice applies to the current session; the next launch uses the system
language again. The interface, dictation and response instructions follow this
choice. The conversation is preserved, and existing messages remain in their
original language.

Repository documentation, source strings and sample conversations use English.
French interface translations are retained in `src/language.hpp`. The host UI
preview uses English by default; set `PREVIEW_FRENCH=1` to preview the French
interface (`PREVIEW_ENGLISH=1` explicitly selects English).

## Dictation

Triangle starts AudioIn capture for the PS5 user, normally through the DualSense
microphone. A headset may change the audio route. Triangle ends capture, which
is limited to 30 seconds. Circle cancels it.

Whisper.cpp 1.8.3 transcribes in the selected language (French or English) on the
PS5 CPU using the multilingual `base` model. Audio stays in console memory;
no transcription API or additional key is required. The text appears in the
prompt for review. **Options sends the text to Codex.** The submitted text is
therefore processed by the online Codex service.

The prototype reserves a 1 GiB native heap and enables Flash Attention on CPU.
The first PS5 test exceeded the previous 512 MiB heap during Whisper
initialization. Dictation now refuses to load the model unless the allocated
heap has at least 768 MiB free. Other allocations retain the fallback to the
system allocator.

AudioIn capture and ChatGPT sign-in have been confirmed on 13.00.
Transcription with this new memory budget still needs confirmation on the console.
The energy filter removes obvious silence, but does not guarantee correct text:
always review the dictation.

## Installing the prototype

After creating the package with `tools/package.py`:

1. Check that the PPSA99105 title ID is available on the console.
2. Copy the complete `bundle/PPSA99105` folder to
   `/data/homebrew/PPSA99105`, following the ShadowMount Folder workflow.
3. Copy `bundle/assistant-service.elf` to
   `/data/codex-ps5/payloads/assistant-service/assistant-service.elf`.
   If this folder is absent, the service also looks for a Payload Manager import
   named `assistant-service` or `assistant-service-*` under `/data/pldmgr/payloads`.
   Also create `/data/ps5-ai-cli`: the legacy ELF loader still uses this
   folder for its lock; Codex data remains under `/data/codex-ps5`.
4. Launch the payload with the Relapse ELF loader (port 9021), or with
   Payload Manager if used, then open Codex PS5 from the library.
   The service must be relaunched after each console reboot.
5. Use L1 to sign in to ChatGPT, then start by testing a request to read files
   in the working directory.

The application folder must contain `assets/ggml-base.bin` (about 148 MB).
The Folder ZIP produced by the build alone does not yet include the model;
use the final `codex-ps5-0.0.2.zip` package.

## Controls

| Button | Action |
| --- | --- |
| Triangle | Start / finish dictation |
| Square | Open / close the controller keyboard |
| Cross | Enter a key / confirm a choice |
| Options | Send the prompt |
| L1 | Sign in to ChatGPT |
| R1 | French / English, only on a French PS5 while idle |
| Circle | Cancel dictation / interrupt Codex / decline an action |
| Up / down | Scroll through the response while the keyboard is closed |

Approval requests for commands and file changes appear in a window with
scrollable details. The client automatically accepts recognized simple
inspection commands: `ls`, `cat`, `head`, `tail`, `wc`, `grep`, `du`, `df`, `ps`,
`pwd`, `uname`, `whoami`, `readlink`, `ps5-tools` and named property reads with
`sysctl`. This acceptance applies only to the requested command, never to the
entire session. Scripts, arbitrary executable paths, compound commands,
redirections, shell expansions and requests for additional permissions still
require confirmation, even if their apparent intent is to read data.
A command missing from the request is never automatically accepted based on
details from a previous command. File changes show the path and diff supplied
by Codex. Missing or excessively long details cannot be approved in this
prototype. The initial choice is **Decline**. The application does not reuse
an earlier Cross button press to approve a new action.

## Local toolbox

The runtime includes the Dash shell, 83 sbase utilities and native `df`, `ps`,
`sysctl` and `ps5-tools` commands. The `ps5-tools` catalog describes the installed
set; Codex chooses commands according to the request.

Tools cover files and links (`cp`, `mv`, `rm`, `find`, `ln`, `readlink`), editing
and text (`ed`, `sed`, `grep`, `sponge`, `strings`), permissions (`chmod`, `chown`,
`chgrp`), archives and checks (`tar`, `dd`, `cksum`, SHA checksums), capacity
(`df`, `du`), processes (`ps`, `kill`, `nice`, `renice`) and system information
(`uname`, `hostname`, `date`, `sysctl`). The source list is in
`backend/runtime-tools.txt`. Options follow sbase rather than all GNU
extensions; an unknown option should produce an error.
Link creation also depends on the filesystem; it was denied in the console's
internal test folder even though the command was available.

`ps` queries the actual Prospero process list and displays PID/name.
It does not expose arguments or CPU statistics. A kernel hook or a payload
that exits after installing its hooks is not necessarily a process:
its absence from `ps` does not prove that it is inactive.
`sysctl` reads named properties; opaque values require `-b`.
This toolbox does not yet provide the full FreeBSD userland: `curl`, `netstat`,
`git`, Python and mounting/unmounting are not included.

## File access and limitations

Codex state and credentials: `/data/codex-ps5/home/.codex`.
Initial working directory: `/data/codex-ps5/workspace`.
The engine runs with the payload's permissions; there is **no system sandbox**.
The Codex `untrusted` policy requests approval for actions not considered safe.
The guidance about `/data`, `/mnt/usb*` and system files consists of model
instructions, not restrictions enforced by the system.
The local socket is unauthenticated: local applications with access to the
console network must be trusted.

This first prototype supports one active conversation with chronological
messages, streaming responses, thinking/replying states and controller input.
Up/down scrolls through session messages (64 recent messages, up to 1 MiB of
text). Conversation resumption after closing, a file browser and a full diff
view are not available yet. A disconnect stops the app-server for that session;
an action already started may continue or may already have modified files.
It is not rolled back. Prompts are never automatically resent.

## Building

Docker must run on a **remote Linux host**; workstation scripts synchronize
sources and retrieve artifacts. Set `PS5_BUILD_HOST` (SSH alias) and
`PS5_BUILD_DIRECTORY` for your build environment.

```sh
bash tools/test.sh
# Optional, with Codex 0.160.0 installed on the workstation:
python3 tests/app_server_handshake.py
PS5_BUILD_HOST=build-host bash tools/build-remote.sh
PS5_BUILD_HOST=build-host bash tools/build-backend-remote.sh
python3 tools/fetch-model.py
python3 tools/package.py
```

The first engine build downloads the SDK and Rust, then compiles Codex;
it takes substantially longer than building the interface. Allow several GB
of RAM and enough disk space. The host must support `sudo -n docker` and have
Python, Git, npm and rsync installed. The native builder uses the Linux image
`opennow-ps5-builder:latest`; if missing, build it on the Linux host with
`docker build -f tools/Dockerfile.native -t opennow-ps5-builder:latest .`.

`upstream-lock.json` and the ps5-ai-cli lockfiles pin revisions. The Whisper
model is downloaded separately and verified with SHA-256, then included by
the packaging script. Model weights are not tracked in version control.

## Attribution

See `THIRD_PARTY.md`, `LICENSE`, `licenses/` and the licenses in the
`vendor/ps5-ai-cli` snapshot. Source for the adaptations and build scripts
is provided.

## Updates through Botty+

With Codex selected, LAUNCH installs and updates the native title, Whisper and
the engine before starting it. Downloads are verified in 1 MiB blocks.
The working directory and ChatGPT sign-in are preserved. Close native
applications before LAUNCH; previous installations are kept as backups.
Engine 0.0.2 exposes a local control endpoint at 127.0.0.1:49323 for status
and clean shutdown while idle. The previous engine requires a full reboot
after the first update.
