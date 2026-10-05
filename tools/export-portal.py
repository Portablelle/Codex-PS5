#!/usr/bin/env python3
"""Export bounded, content-addressed Codex release blocks for the Botty portal."""
import argparse, hashlib, json, shutil
from pathlib import Path
root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--portal-root', required=True, type=Path)
a = p.parse_args()
base = a.portal_root / 'apps/codex'
base.mkdir(parents=True, exist_ok=True)
block_size = 1048576
physical = {}
def record(path, name):
    chunks = []
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        while block := stream.read(block_size):
            digest.update(block)
            sha = hashlib.sha256(block).hexdigest()
            relative = 'chunks/' + sha + '.bin'
            target = base / relative
            target.parent.mkdir(exist_ok=True)
            if not target.exists(): target.write_bytes(block)
            physical[relative] = {'path': relative, 'size': len(block), 'sha256': sha}
            chunks.append(sha)
    return {'path': name, 'size': path.stat().st_size, 'sha256': digest.hexdigest(), 'chunks': chunks}
app = root / 'dist/bundle/PPSA99105'
if not app.is_dir(): raise SystemExit('Run tools/package.py first')
native = [record(path, path.relative_to(app).as_posix()) for path in sorted(app.rglob('*')) if path.is_file()]
service = record(root / 'dist/assistant-service.elf', 'assistant-service.elf')
build = json.loads((root / 'dist/service-build.json').read_text())
version = (root / 'VERSION').read_text().strip()
if build['version'] != version: raise SystemExit('Backend version is stale')
manifest = {'schema': 1, 'version': version, 'titleId': 'PPSA99105', 'serviceBuild': build['build'],
            'chunkSize': block_size, 'native': native, 'service': [service], 'files': list(physical.values())}
blob = (json.dumps(manifest, indent=2) + '\n').encode()
(base / 'manifest.json').write_bytes(blob)
for path in (base / 'chunks').glob('*.bin'):
    if 'chunks/' + path.name not in physical: path.unlink()
shutil.copy2(root / 'LICENSE', base / 'LICENSE')
shutil.copy2(root / 'THIRD_PARTY.md', base / 'NOTICE.md')
# Redistributed dependency notices travel with the executable, without credentials or private sources.
notice = (root / 'THIRD_PARTY.md').read_text()
for tree in (root / 'licenses', root / 'vendor/ps5-ai-cli/LICENSES'):
    for path in sorted(tree.rglob('*')):
        if path.is_file():
            notice += '\n\n---\n' + path.relative_to(root).as_posix() + '\n\n' + path.read_text()
(base / 'NOTICE.md').write_text(notice)
import re
installer = a.portal_root / 'src/codex-install.js'
installer.write_text(re.sub(r"const HASH = '[a-f0-9]{64}'", "const HASH = '" + hashlib.sha256(blob).hexdigest() + "'", installer.read_text()))
payload = dict(service); payload.pop('path'); payload['chunkSize'] = block_size
(a.portal_root / 'src/codex-payload.js').write_text('export const PAYLOAD = ' + json.dumps(payload, indent=2) + ';\n')
print('Exported Codex', version, 'in', len(physical), 'verified blocks')
