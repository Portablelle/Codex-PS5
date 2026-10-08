#!/usr/bin/env python3
"""Build an immutable portal archive from validated artifacts and committed source."""
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile

from package import package


def package_portal(root):
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
    if subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=normal'], cwd=root):
        raise ValueError('Commit all source changes before publishing a portal package')
    package(root)
    with tempfile.TemporaryDirectory(prefix='codex-portal-') as directory:
        stage = Path(directory)
        subprocess.run(['python3', str(root / 'tools/export-portal.py'),
                        '--portal-root', str(stage)], cwd=root, check=True)
        source = stage / 'apps/codex/codex-source.tar.gz'
        subprocess.run(['git', 'archive', '--format=tar.gz', '--prefix=codex-source/',
                        f'--output={source}', commit], cwd=root, check=True)
        files = sorted(path for path in stage.rglob('*') if path.is_file())
        metadata = {
            'schema': 1, 'commit': commit, 'version': (root / 'VERSION').read_text().strip(),
            'sha256': {path.relative_to(stage).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in files},
        }
        (stage / 'codex-release.json').write_text(json.dumps(metadata, indent=2) + '\n')
        output = root / 'dist/codex-portal.tar.gz'
        with output.open('wb') as stream, gzip.GzipFile(fileobj=stream, mode='wb', mtime=0, filename='') as zipped:
            with tarfile.open(fileobj=zipped, mode='w') as archive:
                for path in sorted(stage.rglob('*')):
                    if not path.is_file():
                        continue
                    info = archive.gettarinfo(str(path), arcname=path.relative_to(stage).as_posix())
                    info.uid = info.gid = info.mtime = 0
                    info.uname = info.gname = ''
                    info.mode = 0o644
                    with path.open('rb') as content:
                        archive.addfile(info, content)
    return output


if __name__ == '__main__':
    try:
        print(package_portal(Path(__file__).resolve().parents[1]))
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
