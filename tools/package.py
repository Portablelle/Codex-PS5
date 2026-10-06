#!/usr/bin/env python3
"""Package a complete ShadowMount title and an independently loadable service."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        while block := stream.read(1048576):
            digest.update(block)
    return digest.hexdigest()


def archive(path, directory, paths):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as output:
        for source in sorted(paths):
            output.write(source, source.relative_to(directory).as_posix())


def package(root):
    dist = root / 'dist'
    version = (root / 'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Invalid VERSION')
    candidates = [p for p in dist.rglob('PPSA99105')
                  if (p / 'eboot.bin').is_file() and 'bundle' not in p.relative_to(dist).parts]
    if len(candidates) != 1:
        raise ValueError('Expected one built PPSA99105 title in dist')
    source = candidates[0]
    param = json.loads((source / 'sce_sys/param.json').read_text())
    content_version = '.'.join(f'{int(part):0{width}d}'
                               for part, width in zip(version.split('.'), (2, 3, 3)))
    if param['titleId'] != 'PPSA99105' or param['contentVersion'] != content_version:
        raise ValueError('Native title version is stale')
    service = dist / 'assistant-service.elf'
    if not service.is_file():
        raise ValueError('Build the assistant-service.elf backend first')
    with service.open('rb') as stream:
        if stream.read(6) != b'\x7fELF\x02\x01':
            raise ValueError('Backend must be a 64-bit little-endian ELF')
    build = json.loads((dist / 'service-build.json').read_text())
    if build['version'] != version:
        raise ValueError('Backend version is stale')
    inputs = [root / 'VERSION', root / 'upstream-lock.json',
              *sorted((root / 'backend').glob('*')), *sorted((root / 'tools').glob('*')),
              *sorted((root / 'src').glob('*'))]
    identity = hashlib.sha256(b''.join(p.relative_to(root).as_posix().encode() + b'\0' + p.read_bytes()
                                       for p in inputs if p.is_file())).hexdigest()
    if build['build'] != identity:
        raise ValueError('Backend sources changed: rebuild before packaging')
    model = root / '.deps/models/ggml-small-q5_1.bin'
    lock = json.loads((root / 'upstream-lock.json').read_text())['whisper_model']
    if not model.is_file():
        raise ValueError('Run tools/fetch-model.py first')
    if sha256(model) != lock['sha256']:
        raise ValueError('Model hash mismatch')
    for path in source.rglob('*'):
        if path.is_symlink():
            raise ValueError(f'Symlinks cannot be released: {path}')
    if (source / 'sce_module/libSceAudioIn.prx').exists():
        raise ValueError('AudioIn link stub must not be redistributed')
    bundle = dist / 'bundle'
    if bundle.exists():
        shutil.rmtree(bundle)
    app = bundle / 'PPSA99105'
    app.mkdir(parents=True)
    shutil.copy2(source / 'eboot.bin', app / 'eboot.bin')
    for name in ('sce_sys', 'sce_module', 'assets'):
        if (source / name).is_dir():
            shutil.copytree(source / name, app / name)
    (app / 'assets').mkdir(exist_ok=True)
    shutil.copy2(model, app / 'assets/ggml-small-q5_1.bin')
    # Notices also travel with the independently distributed Folder ZIP.
    notices = app / 'release'
    notices.mkdir()
    for name in ('README.md', 'INSTALL.md', 'THIRD_PARTY.md', 'LICENSE', 'VERSION', 'upstream-lock.json'):
        shutil.copy2(root / name, bundle / name)
        shutil.copy2(root / name, notices / name)
    shutil.copytree(root / 'licenses', bundle / 'licenses')
    shutil.copytree(root / 'vendor/ps5-ai-cli/LICENSES', bundle / 'licenses/ps5-ai-cli')
    shutil.copytree(bundle / 'licenses', notices / 'licenses')
    shutil.copy2(service, bundle / service.name)
    # Preserve the legacy archive and staging layout for Botty exporters.
    archive(dist / f'codex-ps5-{version}.zip', dist, (p for p in bundle.rglob('*') if p.is_file()))
    release = dist / 'release'
    if release.exists():
        shutil.rmtree(release)
    release.mkdir()
    archive(release / f'codex-ps5-{version}-Folder.zip', bundle,
            (p for p in app.rglob('*') if p.is_file()))
    for source in (service, root / 'INSTALL.md', root / 'THIRD_PARTY.md', root / 'LICENSE'):
        shutil.copy2(source, release / source.name)
    shutil.copytree(bundle / 'licenses', release / 'licenses')
    archive(release / 'licenses.zip', release,
            (p for p in (release / 'licenses').rglob('*') if p.is_file()))
    shutil.rmtree(release / 'licenses')
    (release / 'release.json').write_text(json.dumps({
        'version': version, 'titleId': 'PPSA99105', 'serviceBuild': build['build'],
        'modelSha256': lock['sha256'], 'format': 'ShadowMount Folder + ELF',
    }, indent=2) + '\n')
    files = sorted(p for p in release.iterdir() if p.is_file())
    (release / 'SHA256SUMS').write_text(''.join(f'{sha256(p)}  {p.name}\n' for p in files))
    return release


if __name__ == '__main__':
    try:
        print(package(Path(__file__).resolve().parents[1]))
    except (ValueError, FileNotFoundError) as error:
        raise SystemExit(str(error))
