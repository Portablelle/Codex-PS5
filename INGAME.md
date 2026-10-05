# In-game voice companion: first implementation

This is the session core and a host JSONL probe, not a working system-wide PS5
overlay. The existing PS5 application and assistant-service lifecycle are
unchanged. Nothing is injected, installed, or enabled by this implementation.
There is no default controller combination.

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

1. Port a resident event loop/transport to a payload so it owns the gameplay
   session after the graphical application closes. The current service supports
   one connected client and terminates that client's engine on disconnect;
   concurrent native chat and in-game sessions need an explicit ownership design.
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

Host validation: `bash tools/test.sh`; optionally
`python3 tests/app_server_handshake.py` with Codex 0.160.0 installed. Neither test
sends a model turn. PS5 native compilation and live in-game behavior remain
unverified by the host tests.

Protocol reference: [official Codex app-server documentation](https://developers.openai.com/codex/app-server).
The image input was also checked against the schema generated by the project's
Codex 0.160.0 version (`codex app-server generate-json-schema`).
