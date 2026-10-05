#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,urllib.request
root=Path(__file__).resolve().parents[1]
model=json.loads((root/'upstream-lock.json').read_text())['whisper_model']
p=root/'.deps/models/ggml-base.bin';p.parent.mkdir(parents=True,exist_ok=True)
if not p.exists():
 temporary=p.with_suffix('.download')
 urllib.request.urlretrieve(model['url'],temporary)
 if hashlib.sha256(temporary.read_bytes()).hexdigest()!=model['sha256']:
  temporary.unlink();raise SystemExit('Model hash mismatch')
 temporary.rename(p)
if hashlib.sha256(p.read_bytes()).hexdigest()!=model['sha256']:raise SystemExit('Model hash mismatch')
print(p)
