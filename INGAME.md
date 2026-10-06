# In-game voice companion: resident prototype

This includes the session core, a separate resident ELF payload, and a host JSONL
probe. It is not a working system-wide PS5 overlay. The native application is
unchanged; assistant-service now supports two independent client connections.
Nothing is automatically injected, installed, or enabled. There is no default
controller combination.

## Implemented

`src/ingame.cpp` owns a gameplay-specific Codex protocol and the complete
asynchronous interaction lifecycle. It requests a screenshot before displaying
the listening indicator, records up to 30 seconds, hides the indicator during
transcription and inference, and emits the completed answer for spoken playback.
Triggering again ends recording; triggering during playback stops speech.
Triggering during capture, transcription, inference, or cancellation does nothing.
Separate cancellation is available throughout the interaction.

Questions include a `localImage` attachment in the same `turn/start` input as the
transcript. Follow-up questions use the same gameplay thread with a fresh image.
The normal file assistant retains its existing instructions and approvals. The
gameplay thread requests `read-only` / `never`, uses instructions for concise
spoken game advice, and declines any command or file-change approval requests.
This is a Codex configuration, not an additional OS sandbox on the PS5.

Every platform action and callback carries an increasing request ID. Callbacks
for cancelled or superseded requests, or the wrong phase, are ignored. A failed
turn never speaks its partial answer. A completed turn with an empty or over-long
(>16384 bytes) answer ends with the status "No usable answer" and nothing is
spoken. Disconnects do not resend questions.
Cancellation during inference waits for termination before releasing the image;
if it cannot terminate within 10 seconds, it requests a transport disconnect.

Timeouts: capture 5 seconds, recording 30 seconds, transcription 120 seconds,
inference 180 seconds, playback 120 seconds. Call `tick()` regularly with a
monotonic millisecond clock. Calls must run on one event loop. Drain both output
queues after every call; their ownership belongs to that loop.

## Resident payload and transport

`backend/ingame-service.cpp` runs an independent event loop and owns the Codex
connection and `InGameSession`. It connects to assistant-service on
`127.0.0.1:49322` and listens for local control/adapter connections on
`127.0.0.1:49324`. Closing an observer window does not close this engine, lose
conversation state, or interrupt inference. This is independent of the graphical
application. Persistence is within the running payload, not across its restart
or a console reboot.

The backend serves up to two clients, each with its own app-server child and
bounded duplex buffers. This allows the native app and resident companion to
coexist. Disconnecting either terminates only its own child. A third client is
closed rather than queued for another engine. The HTTP control endpoint remains
available while clients are connected; `/status` reports `connections` and an
accurate `idle` value. `/stop` returns HTTP 409 while any owner is connected.
Stop the resident companion and close the native app before updating the engine.

Resident peers send a newline-terminated hello first:

```json
{"command":"hello","role":"observer"}
{"command":"hello","role":"adapter"}
```

These are alternatives for separate connections. Up to four peers can connect,
including **one** media adapter. Observers receive status only and may query
`status` or request an idle `stop`; they cannot trigger recording, advertise
capabilities, inject engine replies, or return media callbacks. An incomplete
hello is disconnected after two seconds. The endpoints trust local applications,
as does the existing engine socket; roles and instance IDs are not authentication.
No network-facing listener is added.

The initial status includes an `instance` identifier. Subsequent commands that
change state must include that value. Each emitted platform action includes
`instance`, `request`, and `sequence`. Adapter commands:

```json
{"command":"capabilities","instance":"FROM_STATUS","capabilities":{"overlay":true,"capture":true,"microphone":true,"speech":true}}
{"command":"trigger","instance":"FROM_STATUS"}
{"command":"cancel","instance":"FROM_STATUS"}
{"command":"adapter","instance":"FROM_STATUS","message":{"event":"captured","request":1}}
{"command":"ack","instance":"FROM_STATUS","sequence":1}
```

`capabilities` starts false and must be advertised after every adapter connection.
On media adapter loss, capabilities reset and the question is cancelled, while
its Codex connection and thread remain owned by the payload. Captures, recording,
and speech actions are never replayed. Cleanup actions (`cancelMedia`,
`stopSpeech`, hidden `overlay`, `releaseCapture`) are retained until `ack` confirms
execution. They are replayed with the same sequence on adapter reconnect and must
be idempotent. Acknowledge them **after** cancellation joins and file cleanup,
not simply after receiving bytes. New work follows retained cleanup in transport
order. The payload refuses an intentional `stop` until the active request and
outstanding cleanup have finished.

Input and queued output are bounded to 1 MiB per stream. Slow or malformed peers
are disconnected without resending questions. At most 32 cleanup actions can be
outstanding; exceeding this limit terminates the payload with failure rather
than silently discarding cleanup. A crashed or forcibly terminated payload still
requires the adapter to stop its media workers on socket loss; cleanup is not
journaled across payload crashes. Snapshot paths have an instance-specific
subdirectory to avoid reuse across launches. The adapter owns directory creation.

Engine transport loss cancels the active request without replay. Reconnection
starts a fresh gameplay thread and is retried only while the core is idle.
Existing ChatGPT credentials are read by the engine; sign in with the native
application before starting the companion. If launched before sign-in, stop and
restart the companion after login. The payload exposes no login codes or account
credentials to observers.

## Build and inspect

```sh
make ingame-host
build/ingame-service --capture-directory /tmp/codex-ps5-captures
# In another host terminal:
python3 tools/ingame-control.py status
python3 tools/ingame-control.py watch
python3 tools/ingame-control.py stop

# PS5 ELF build, using the existing codex-ps5-rust:0.1 Docker image on the VPS:
make ingame
```

The host binary uses real sockets and real app-server protocol messages. It does
not synthesize engine replies or implement any capture/audio operations; use the
fake-engine integration test for an offline demonstration. A host run on the
default port requires an engine bridge on port 49322. The control CLI is host-only;
Python is not added to the PS5.

`make ingame` builds in a separate remote directory (default
`/home/ubuntu/codex-ps5-ingame`, configurable with `PS5_INGAME_BUILD_DIRECTORY`) and
retrieves `dist/ingame-service.elf`. It also compile-checks the modified C backend,
without rebuilding or replacing the existing assistant-service ELF. Rebuild the
backend separately with `make backend` for the two-client behavior. The new
payload is experimental and is not added to the automatic installer/package.
Once separately installed, it can be launched through the existing ELF loader or
Payload Manager alongside the updated assistant-service. No live console launch
has been verified in this change.

## Platform adapter contract

All four capabilities (`overlay`, `capture`, `microphone`, `speech`) start false.
Activation refuses to run until an adapter explicitly advertises them and the
Codex thread is ready. The core never assumes that opening the native app makes
these capabilities available while a game is running.

Actions are JSON objects with `action` and `request` fields:

| Action | Adapter responsibility |
| --- | --- |
| `capture`, `path` | Write the current game image as a PNG to this exact path, then report `captured`. |
| `overlay`, `visible`, optional `status` | Show a small listening indicator without taking controller focus or suspending the game; hide it when `visible` is false. |
| `record`, `maxSeconds` | Start the user's microphone and stop within the limit. |
| `stopRecording` | Finish the current recording and start transcription. |
| `cancelMedia` | Cancel and join capture, recording, and transcription before later cleanup actions execute. |
| `releaseCapture`, `path` | Remove only the snapshot created for this request. |
| `speak`, `text` | Synthesize and play the completed answer alongside game audio, then report `speechFinished`. |
| `stopSpeech` | Stop and join playback. |
| `disconnectEngine` | Close the Codex transport before executing subsequent cleanup actions. |

Adapter callbacks are objects with `event` and `request`. Supported events:
`captured`, `recordingStopped`, `transcribed` (with `text`), `speechFinished`,
and `failed`. Report `recordingStopped` before `transcribed` if recording ended
automatically; it is unnecessary after the core emitted `stopRecording`.
Report a failed overlay, microphone, transcription, capture, or playback through
`failed` rather than pretending that operation succeeded.

Execute actions in order, with cancellation/cleanup barriers as specified above.
Ignore or abort media work whose request has already been cancelled. Create a
dedicated capture directory, keep it free of symlinks, and atomically replace a
snapshot before acknowledging `captured`. Do not accept another path in a
callback. Captures must be accessible to the Codex engine on the same filesystem.
Only the capture adapter owns files; the core does not open or delete them.

`InGameShortcut` accepts a caller-supplied button mask and detects one activation
per chord press. Feed every connected pad sample, including releases; reset it on
disconnect and suppress activation from a reconnect's initial held-button state.
The global pad hook and the eventual choice of combination are separate work.

## Run the host probe

```sh
make ingame-probe
```

The probe reads JSONL commands and emits `codex`, `platform`, and `status`
channels. It simulates no operations automatically and contacts neither the
console nor OpenAI. `codex` messages can be inspected as actual app-server JSON-RPC
requests; input `engine` commands represent its replies. This probe does not own
a real transport and is not a resident daemon. Input commands:

```json
{"command":"capabilities","overlay":true,"capture":true,"microphone":true,"speech":true}
{"command":"engine","message":{"id":1,"result":{}}}
{"command":"engine","message":{"id":2,"result":{"account":{"type":"chatgpt"}}}}
{"command":"engine","message":{"id":3,"result":{"thread":{"id":"simulated-game"}}}}
{"command":"trigger"}
{"command":"adapter","message":{"request":1,"event":"captured"}}
{"command":"trigger"}
{"command":"adapter","message":{"request":1,"event":"transcribed","text":"How do I beat this monster?"}}
{"command":"engine","message":{"id":4,"result":{"turn":{"id":"simulated-turn"}}}}
{"command":"engine","message":{"method":"item/completed","params":{"item":{"type":"agentMessage","id":"answer","text":"Dodge left, then attack."}}}}
{"command":"engine","message":{"method":"turn/completed","params":{"turn":{"status":"completed"}}}}
{"command":"adapter","message":{"request":1,"event":"speechFinished"}}
```

Other commands: `cancel`, `disconnected`, `tick`. The probe uses the actual host
clock; `tick` advances timeout handling to that time, without simulating time.
Generated paths identify future PS5 snapshots; the probe creates no image files.

## Remaining console work

1. Validate the resident ELF's launch, lifetime, engine connection, and the
   updated two-client backend on the console. Measure the memory cost of two
   independent engines; host tests do not establish console performance.
2. Validate a ShellUI overlay and global controller hook on firmware 13.00 with
   Relapse/kstuff/ShadowMount. Existing etaHEN overlay code is a research reference,
   not a required or installed dependency.
3. Validate capture of the active game's framebuffer without switching apps.
4. Move microphone capture and transcription out of the app. Measure interference
   with game audio/headsets and the CPU/memory impact of Whisper before choosing
   transcription settings for gameplay.
5. Implement a speech provider and PS5 audio mixing/playback. No speech provider,
   API subscription, credentials, or voice have been selected or added.
6. Connect activation in the native app only when these capabilities work. Test
   across the home screen, PS4/PS5 games, headset routing, suspend/resume, and
   disconnects before claiming system-wide compatibility.

Host validation: `bash tools/test.sh` includes real socket integration tests with
fake engines; optionally `python3 tests/app_server_handshake.py` with Codex 0.160.0
installed. No real model turn is sent. The resident payload was cross-compiled
with the PS5 SDK and the modified backend was target compile-checked. Live
console execution, native application compilation, and in-game behavior remain
unverified.

Protocol reference: [official Codex app-server documentation](https://developers.openai.com/codex/app-server).
The image input was also checked against the schema generated by the project's
Codex 0.160.0 version (`codex app-server generate-json-schema`).
