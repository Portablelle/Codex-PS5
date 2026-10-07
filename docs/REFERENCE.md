# Codex PS5 0.0.4 — native prototype

An assistant for files on a jailbroken PS5, with a native controller interface
and local dictation. This project is not an official OpenAI or Sony application.
The validation target is firmware 13.00 with Relapse/kstuff/ShadowMount.
ChatGPT sign-in, local commands and chat have been tested on this console.

## How it works

Two components run **on the PS5**:

* `assistant-service.elf`: the Codex 0.160.0 engine ported by
  [ps5-ai-cli](https://github.com/saawant12/ps5-ai-cli), adapted to expose
  `codex app-server` on a **127.0.0.1:49322** socket. Shell tools and file
  modifications run locally.
* `PPSA99105`: a native graphical application built with tools from
  [OpenNOW PS5](https://github.com/Portablelle/opennow-ps5) and the Botty+ renderer.
  It uses the app-server JSONL protocol without a browser.

The Botty portal integration can install/update and start Codex through its
**Codex PS5** option when matching assets are published there. The engine can
also be installed manually and launched with the Relapse ELF loader or Payload
Manager. GitHub release assets do not update the portal itself.

The PPSA99105 title ID is provisional: check that it is available before the
first installation. The build scripts do not install anything on the console.

Sign in to ChatGPT using a code displayed on the TV and confirmed on a phone.
Codex responses require Internet access and an account with Codex access.
The selected model is **GPT-6.1 SOL**, with **low** reasoning effort.
The application does not include an SSH connection.

## Language

At startup, the PS5 system language determines the interface language: French
(France or Canada) on a French console, and English for all other languages
or if reading the setting fails. On a French console only, **R1** switches
between French and English while Codex and the microphone are idle.
The choice applies to the current session; the next launch uses the system
language again. This changes only interface text and the controller keyboard
layout; it sends no request to OpenAI
and preserves the signed-in account and the exact conversation. Messages are
sent as entered, and Codex keeps the conversation language unless the user asks
for another language. Dictation detects the spoken language automatically.

Repository documentation, source strings and sample conversations use English.
French interface translations are retained in `src/language.hpp`. The host UI
preview uses English by default; set `PREVIEW_FRENCH=1` to preview the French
interface (`PREVIEW_ENGLISH=1` explicitly selects English). `PREVIEW_KEYBOARD`,
`PREVIEW_APPROVAL`, `PREVIEW_DECISION`, `PREVIEW_QUIT`, `PREVIEW_THINKING` and `PREVIEW_SIGNIN`
render the other screens, and `PREVIEW_STATUS` overrides the status text.

## Dictation

Triangle starts AudioIn capture for the PS5 user, normally through the DualSense
microphone. A headset may change the audio route. Triangle ends capture, which
is limited to 30 seconds. Circle cancels it.

Whisper.cpp 1.8.3 automatically detects the spoken language and transcribes on the
PS5 CPU using the multilingual `small` model quantized to Q5_1. Audio stays in
console memory;
no transcription API or additional key is required. The text appears in the
prompt for review. **Options sends the text to Codex.** The submitted text is
therefore processed by the online Codex service.

Transcription uses six CPU threads and greedy decoding, with automatic language
detection and no initial vocabulary prompt.

The prototype reserves a 1 GiB native heap and enables Flash Attention on CPU.
The first PS5 test exceeded the previous 512 MiB heap during Whisper
initialization. Dictation now refuses to load the model unless the allocated
heap has at least 768 MiB free. Other allocations retain the fallback to the
system allocator.

AudioIn capture, ChatGPT sign-in and Small Q5_1 transcription with six CPU
threads have been confirmed on 13.00. Console testing found better recognition
than Base, with longer transcription times.
The energy filter removes obvious silence, but does not guarantee correct text:
always review the dictation.

## Installing and updating

See [INSTALL.md](../INSTALL.md) for public release downloads, PS5 Upload / FTP
installation, updates, and rollback. The complete Folder ZIP includes Whisper
Small Q5_1; the intermediate native build ZIP does not.

## Controls

An experimental in-game voice companion core, resident ELF payload, and host
probe are available; see [INGAME.md](../INGAME.md). The payload owns its conversation
independently of the native window; the backend supports two independent clients.
System-wide overlay, game capture, microphone/transcription, and spoken playback
are not connected to the payload yet. No new controller shortcut is enabled.

| Button | Action |
| --- | --- |
| Triangle | Start / finish dictation |
| Square | Open / close the controller keyboard |
| Cross | Enter a key / confirm a choice |
| Options | Send the prompt (`/quit` opens the exit confirmation, `/new` starts a new conversation) |
| Touchpad click | Open the exit confirmation |
| L1 | Sign in to ChatGPT |
| R1 | French / English, only on a French PS5 while idle |
| Circle | Cancel dictation / interrupt Codex / decline an action |
| Up / down | Scroll through the response while the keyboard is closed |

The controller keyboard uses AZERTY in French and QWERTY in English. Switching
the interface language with R1 also switches the keyboard layout immediately.

To close the application, click the touchpad or enter `/quit` and press Options.
The confirmation defaults to **Cancel**; select **Quit** with left/right and
press Cross, or press Circle to go back. It is available even while disconnected
or during dictation, a response or an approval request. Confirming stops dictation,
declines pending approvals, requests interruption of the active turn and closes
the connection before releasing application resources and exiting. An action
already started is not rolled back; the conversation resumes at the next launch.

Approval requests for commands and file changes appear in a window with
scrollable details. Under the title, short plain-language lines describe the
request: what the command does (for example "Moving a file" for `mv`,
"Renaming a file" when both paths share a folder, "Deleting folders and their
contents" for `rm -r`, "Extracting an archive" for `tar x`), what a file change
does (creating, editing, deleting, moving or renaming files, from the change
kinds supplied by Codex), and any extra access it asks for (write access to a
folder, network access, additional permissions, or input sent to a running
command). Command summaries cover the bundled toolbox and common programs, look
through `sh -c`/`-lc` wrappers, `env`, `nice`, `nohup` and a leading `cd dir &&`,
and fall back to "Running a program" for anything else, including a tool name
run from a path outside the system and bundled runtime directories, a command
that sets `PATH`, `LD_*` or another variable that changes what runs, and a wrapper or shell option the summary does not know. Commands joined with
`;`, `&&`, `||`, `|`, `&` or command substitution are summarized as "Running
several commands", as is any command that combines one of these separators
with a backslash, `#` or `$`, even inside quotes. Globs and recursive options use
the plural or folder wording. Redirection to a file is "Writing to a file". The
summary is only a reading aid: it never affects which commands are accepted, so
always check the full command below it. The client automatically accepts recognized simple
inspection commands: `ls`, `cat`, `head`, `tail`, `wc`, `grep`, `du`, `df`, `ps`,
`pwd`, `uname`, `whoami`, `readlink`, `ps5-tools` and named property reads with
`sysctl`. This acceptance applies only to the requested command, never to the
entire session. The bundled runtime shell's `-c` and `-lc` wrappers are also
accepted when their sole command argument passes the same inspection filter
(for example, `…/runtime/tools-<32-hex-id>/sh -lc 'df -h'`). Other shell paths,
nested wrappers, scripts, arbitrary executable paths, compound commands,
redirections, shell expansions and requests for additional permissions still
require confirmation, even if their apparent intent is to read data.
A command missing from the request is never automatically accepted based on
details from a previous command. File changes show the path and diff supplied
by Codex. When a request refers to a command or file change announced earlier
in the turn, its details stay available until that item finishes, however many
other commands the turn runs. Missing or excessively long details cannot be
approved in this prototype. The initial choice is **Decline**. The application
does not reuse an earlier Cross button press to approve a new action.

After each choice, the conversation keeps a short line with the decision and
the summary, for example "Allowed: Moving a file" or "Declined: Deleting a
file" ("A command" or "A file change" when there is no summary). It records the
decision actually sent: an action with incomplete details is always
"Declined", and so are the requests declined on quit. Commands accepted
automatically leave no line.

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
text). A file browser and a full diff view are not available yet.

The app remembers the Codex thread of the last conversation once a prompt has
been sent in it; only its ID is stored, in the title's own storage
(`/download0/codex-ps5-conversation`), while Codex keeps the conversation in
`/data/codex-ps5/home`. At the next launch, and after a reconnection to the
engine, the app resumes that thread with `thread/resume` and reads **Conversation
resumed**. It then shows the last 20 turns at most (64 messages, 1 MiB), each as
its first prompt and last reply, fetched one turn per request so that no answer
exceeds the 1 MiB line limit; prompts can be sent meanwhile. Approval decisions,
commands, file changes and intermediate replies of earlier turns are not shown again.

If Codex cannot resume it, the app starts a new conversation and shows why above
the composer, with the reason given by Codex. If a resume ends the connection, it
is not repeated in a loop: the reconnection starts a new conversation. In both
cases the remembered conversation is kept, and tried again at the next launch,
until a prompt is sent in the new one. After a disconnect the app waits 1.5 s
before reconnecting, so the previous engine has exited before the conversation
is opened again.

To leave a conversation, enter `/new` and press Options while Codex is idle: the
screen is cleared and a new conversation starts; the previous one stays in the
Codex state but is no longer resumed. The in-game companion never resumes a
conversation.

A disconnect stops the app-server for that session; an action already started
may continue or may already have modified files. It is not rolled back. A turn
interrupted by a disconnect is not continued, and prompts are never automatically resent.
If Codex cannot start a conversation (for example when the model is
unavailable), the error stays in the status line and the Send button reads **Retry**.
Pressing Options starts
a new attempt and keeps the prompt in the composer; send it once the status
returns to **Ready**. The in-game shortcut retries the same way and reports
"Codex is not ready" meanwhile.

## Building

Docker must run on a **remote Linux host**; workstation scripts synchronize
sources and retrieve artifacts. Set `PS5_BUILD_HOST` (SSH alias) and
`PS5_BUILD_DIRECTORY` for your build environment. `build-remote.sh` and
`build-backend-remote.sh` mirror the checkout into `PS5_BUILD_DIRECTORY` and delete
files that are no longer in it (except `.deps`, `build` and `dist`). Use a dedicated
directory: they refuse a non-empty one that holds neither their
`.codex-ps5-build` marker nor an earlier Codex PS5 checkout.

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

See [THIRD_PARTY.md](../THIRD_PARTY.md), [LICENSE](../LICENSE), `licenses/`,
and the licenses in the `vendor/ps5-ai-cli` snapshot (paths relative to the repository root). Source for the adaptations and build scripts
is provided.

## Updates through Botty+

With Codex selected, LAUNCH installs and updates the native title, Whisper and
the engine before starting it. Downloads are verified in 1 MiB blocks.
The working directory and ChatGPT sign-in are preserved. Close native
applications before LAUNCH; previous installations are kept as backups.
The engine exposes a local control endpoint at 127.0.0.1:49323 for status
and clean shutdown while idle. With the resident prototype backend, stop the
companion and close the native app before updating; an active connection makes
`POST /stop` return HTTP 409. The previous engine requires a full reboot
after the first update.
