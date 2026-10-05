#!/usr/bin/env python3
"""Bundle the built native title, backend payload, and local voice model."""
from pathlib import Path
import hashlib,json,re,shutil
root=Path(__file__).resolve().parents[1]
dist=root/'dist'
version=(root/'VERSION').read_text().strip()
if not re.fullmatch(r'\d+\.\d+\.\d+',version):raise SystemExit('Invalid VERSION')
# Folder builds from the platform tool use dist/Folder/<title>.
candidates=[p for p in dist.rglob('PPSA99105') if (p/'eboot.bin').is_file() and 'bundle' not in p.parts]
if len(candidates)!=1:raise SystemExit('Expected one built PPSA99105 title in dist')
service=dist/'assistant-service.elf'
model=root/'.deps/models/ggml-small-q5_1.bin'
lock=json.loads((root/'upstream-lock.json').read_text())['whisper_model']
if not service.is_file():raise SystemExit('Build the assistant-service.elf backend first')
if not model.is_file():raise SystemExit('Run tools/fetch-model.py first')
if hashlib.sha256(model.read_bytes()).hexdigest()!=lock['sha256']:raise SystemExit('Model hash mismatch')
bundle=dist/'bundle';bundle.mkdir(exist_ok=True)
app=bundle/'PPSA99105';shutil.copytree(candidates[0],app,dirs_exist_ok=True)
(app/'assets').mkdir(exist_ok=True);shutil.copy2(model,app/'assets/ggml-small-q5_1.bin')
shutil.copy2(service,bundle/service.name)
for p in ('README.md','THIRD_PARTY.md','LICENSE','VERSION','upstream-lock.json'):shutil.copy2(root/p,bundle/p)
shutil.copytree(root/'licenses',bundle/'licenses',dirs_exist_ok=True)
shutil.copytree(root/'vendor/ps5-ai-cli/LICENSES',bundle/'licenses/ps5-ai-cli',dirs_exist_ok=True)
print(bundle)
archive=shutil.make_archive(str(dist/f'codex-ps5-{version}'), 'zip', dist, 'bundle')
print(archive)
