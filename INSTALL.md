# Install and update Codex PS5

## Downloads

From one [GitHub release](https://github.com/Portablelle/Codex-PS5/releases), get:

- `codex-ps5-0.0.4-Folder.zip`: the complete native title, including Whisper Small Q5_1.
- `assistant-service.elf`: the engine payload. Both downloads are required.
- Optional: `SHA256SUMS` to verify downloads. `licenses.zip`, `LICENSE`, and
  `THIRD_PARTY.md` contain notices for the standalone payload; the Folder ZIP
  already includes the notices.

Use the release assets rather than GitHub's automatically generated source ZIP.
The source archive has no built application or voice model.

## Requirements

The validation setup is PS5 firmware **13.00**, jailbroken with **Relapse**, with
**kstuff / ShadowMount** enabled. An ELF loader must listen on **9021**.
Other firmware combinations have not been confirmed for Codex PS5. You need
Internet access and a ChatGPT account with Codex access for replies.

Use [PS5 Upload](https://github.com/phantomptr/ps5upload) or an FTP client.
For PS5 Upload, follow its connection guide to load and pair its own transfer
helper first. Codex's service is a separate payload.
For FTP, enable your console's FTP server and use its configured port.
Before the first install, check that title ID **PPSA99105** is not already used
by another application.

## First installation

1. Extract `codex-ps5-0.0.4-Folder.zip` on your computer. It contains **PPSA99105**
   directly at the archive root.
2. Upload the complete **PPSA99105** folder into **`/data/homebrew`**.
   With PS5 Upload, use a folder transfer; with FTP, drag the folder into that
   remote directory. Do not add another `PPSA99105` nesting level.
3. Create **`/data/codex-ps5/payloads/assistant-service`** and upload
   **`assistant-service.elf`** to it. Copy it to disk **before** sending it to
   the ELF loader: the running service needs its on-disk image to install the runtime.
4. Create **`/data/ps5-ai-cli`** if it does not exist. This empty directory is used
   for a legacy loader lock; your Codex data lives separately in `/data/codex-ps5`.
5. Wait for all transfers to finish. Keep kstuff and ShadowMount active and let
   ShadowMount scan/register the new title.
6. Use PS5 Upload's payload sender (or your existing ELF sender) to send the
   **same downloaded `assistant-service.elf`** to your PS5's ELF loader on **9021**.
7. Launch **Codex PS5** from the console library. Press **L1**, then confirm
   the displayed ChatGPT sign-in code on your phone. Try asking for available
   disk space before making changes to files.

Your installation should have this structure:

```text
/data/
├── homebrew/
│   └── PPSA99105/
│       ├── eboot.bin
│       ├── sce_sys/
│       ├── sce_module/
│       ├── assets/ggml-small-q5_1.bin
│       └── release/                  # notices and documentation
├── codex-ps5/
│   └── payloads/assistant-service/assistant-service.elf
└── ps5-ai-cli/                       # loader lock directory
```

The app and service are two components. Transferring files does not start the
service. Loading only the ELF does not install the native title or voice model.
**Reload the service after every console reboot.**

### Payload Manager alternative

You can import the ELF into Payload Manager under `assistant-service` (or an
`assistant-service-*` directory) and launch it there. The service recognizes
those imports under `/data/pldmgr/payloads` if `/data/codex-ps5/payloads` is absent.
Use one service location consistently: if the Codex payload directory exists,
keep the matching ELF there even when launching through Payload Manager.

### What about PKG, FFPKG, and Homebrew Launcher?

This release uses the native **ShadowMount Folder** workflow. It is compatible
with folder transfers through PS5 Upload or FTP. An `.ffpkg` is a mountable
filesystem image; a real `.pkg` uses a different installer path. Neither format
removes this application's separate engine requirement.

Do not rename the ZIP to `.pkg` or pass it to a PKG installer. The websrv Homebrew
Launcher expects an `eboot.elf` payload layout; this title instead contains a
native `eboot.bin`. Use ShadowMount for the title, and an ELF loader for the service.

## Updates

1. Close the native app and any experimental companion. **Reboot the console**
   to stop the old engine, then reactivate the jailbreak, kstuff, ShadowMount,
   and ELF loader. Do not start Codex yet.
2. Save a backup of `/data/homebrew/PPSA99105` and the installed
   `assistant-service.elf` on your computer. Keep app backups outside
   ShadowMount's scan folders to avoid duplicate registration.
3. Replace the whole `/data/homebrew/PPSA99105` folder with the new release's
   folder. A clean replacement avoids carrying obsolete files into the new app.
4. Replace `/data/codex-ps5/payloads/assistant-service/assistant-service.elf`
   with the new release's payload (or update the Payload Manager import if that
   is your chosen location).
5. Wait for transfers and registration to finish, send the new ELF, and launch
   the app. Keep the title and service from the **same release**.

**Preserve these folders:**

| Path | Contents |
| --- | --- |
| `/data/codex-ps5/home` | ChatGPT credentials and Codex state (`.codex` is hidden) |
| `/data/codex-ps5/workspace` | Your working files |

Do not delete the entire `/data/codex-ps5` directory. The app resumes its last
conversation from the Codex state in `/data/codex-ps5/home`; if that conversation
cannot be resumed after an update, a new one starts automatically.

To roll back, close the app, reboot, and restore **both** the backed-up title
folder and its matching service ELF. Preserve the same user-data folders.

Botty+ users can keep using their existing Codex LAUNCH integration when its
maintainer publishes a matching update. These GitHub assets do not themselves
update the Botty portal.

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No library tile | ShadowMount is active; `sce_sys/param.json` is at `/data/homebrew/PPSA99105/sce_sys/param.json`; there is no extra folder level; title ID is available. |
| Engine unavailable | Send the service ELF after reboot. The identical ELF must already be at the documented on-disk path. Check the legacy lock directory exists. |
| Dictation cannot load | The full `assets/ggml-small-q5_1.bin` model was transferred. Use the release Folder ZIP, not the smaller intermediate build ZIP. |
| Sign-in or replies fail | Check Internet connectivity and Codex access on the account. Press L1 to sign in. |
| Problems after update | Reboot to remove the old service, then load the new one. Check both downloads came from the same release. |

## Verify downloads (optional)

Place the downloads and `SHA256SUMS` in one directory. Check the entry for each
file you downloaded. With all assets present:

```sh
# Linux
sha256sum -c SHA256SUMS
# macOS
shasum -a 256 -c SHA256SUMS
```

On Windows, `Get-FileHash .\assistant-service.elf -Algorithm SHA256` returns
its hash for comparison with the matching `SHA256SUMS` entry.
