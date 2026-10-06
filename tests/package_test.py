#!/usr/bin/env python3
"""Exercise public archive layout, repeat packaging, and release input rejection."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]

spec = importlib.util.spec_from_file_location('package', ROOT / 'tools/package.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PackageTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for path in ('dist/PPSA99105/sce_sys', 'dist/PPSA99105/assets', 'dist/PPSA99105/sce_module',
                     '.deps/models', 'licenses', 'vendor/ps5-ai-cli/LICENSES', 'src', 'backend', 'tools',
                     'docs/images', 'sce_sys'):
            (self.root / path).mkdir(parents=True)
        for name in ('INSTALL.md', 'CHANGELOG.md', 'INGAME.md', 'THIRD_PARTY.md', 'LICENSE', 'docs/REFERENCE.md',
                     'docs/images/shot.png', 'sce_sys/icon0.png',
                     'licenses/native.txt', 'vendor/ps5-ai-cli/LICENSES/engine.txt'):
            (self.root / name).write_text('fixture notice\n')
        (self.root / 'README.md').write_text('<img src="sce_sys/icon0.png">\n![shot](docs/images/shot.png)\n'
                                             '[Guide](INSTALL.md#updates) [Online](https://example.com/x.md)\n')
        (self.root / 'VERSION').write_text('0.0.3\n')
        (self.root / 'src/main.cpp').write_text('fixture build input')
        (self.root / 'dist/PPSA99105/sce_sys/param.json').write_text(json.dumps({
            'titleId': 'PPSA99105', 'contentVersion': '00.000.003'}))
        (self.root / 'dist/PPSA99105/eboot.bin').write_bytes(b'native fixture')
        (self.root / 'dist/PPSA99105/sce_module/libc.prx').write_bytes(b'runtime fixture')
        (self.root / 'dist/assistant-service.elf').write_bytes(b'\x7fELF\x02\x01payload fixture')
        model = b'verified model fixture'
        (self.root / '.deps/models/ggml-small-q5_1.bin').write_bytes(model)
        (self.root / 'upstream-lock.json').write_text(json.dumps({
            'whisper_model': {'sha256': hashlib.sha256(model).hexdigest()}}))
        inputs = [self.root / 'VERSION', self.root / 'upstream-lock.json', self.root / 'src/main.cpp']
        identity = hashlib.sha256(b''.join(p.relative_to(self.root).as_posix().encode() + b'\0' + p.read_bytes()
                                           for p in inputs)).hexdigest()
        (self.root / 'dist/service-build.json').write_text(json.dumps({'version': '0.0.3', 'build': identity}))

    def test_complete_release_and_clean_rebuild(self):
        # Accidental top-level files are not copied into the public title.
        (self.root / 'dist/PPSA99105/.env').write_text('not public')
        release = module.package(self.root)
        stale = self.root / 'dist/bundle/PPSA99105/assets/obsolete.bin'
        stale.write_bytes(b'obsolete')
        (release / 'old.zip').write_bytes(b'obsolete')
        module.package(self.root)
        self.assertFalse(stale.exists())
        self.assertFalse((release / 'old.zip').exists())
        with zipfile.ZipFile(release / 'codex-ps5-0.0.3-Folder.zip') as archive:
            names = set(archive.namelist())
            self.assertIn('PPSA99105/eboot.bin', names)
            self.assertIn('PPSA99105/sce_module/libc.prx', names)
            self.assertIn('PPSA99105/release/licenses/ps5-ai-cli/engine.txt', names)
            self.assertEqual(archive.read('PPSA99105/assets/ggml-small-q5_1.bin'), b'verified model fixture')
            self.assertTrue(all(name.startswith('PPSA99105/') for name in names))
            self.assertNotIn('PPSA99105/.env', names)
        for line in (release / 'SHA256SUMS').read_text().splitlines():
            digest, name = line.split('  ')
            self.assertEqual(module.sha256(release / name), digest)
        self.assertEqual((release / 'assistant-service.elf').read_bytes(),
                         (self.root / 'dist/assistant-service.elf').read_bytes())

    def test_release_documents_resolve_inside_title(self):
        with zipfile.ZipFile(module.package(self.root) / 'codex-ps5-0.0.3-Folder.zip') as archive:
            names = set(archive.namelist())
        for name in ('README.md', 'INSTALL.md', 'CHANGELOG.md', 'INGAME.md', 'docs/REFERENCE.md',
                     'docs/images/shot.png', 'sce_sys/icon0.png'):
            self.assertIn(f'PPSA99105/release/{name}', names)

    def test_reject_broken_document_link(self):
        (self.root / 'docs/REFERENCE.md').write_text('[Missing](../MISSING.md)\n')
        with self.assertRaisesRegex(ValueError, 'Broken link in docs/REFERENCE.md: ../MISSING.md'):
            module.package(self.root)

    @unittest.skipUnless(hasattr(os, 'symlink'), 'symlinks unavailable')
    def test_reject_symlink(self):
        os.symlink('eboot.bin', self.root / 'dist/PPSA99105/alias.bin')
        with self.assertRaisesRegex(ValueError, 'Symlinks cannot be released'):
            module.package(self.root)

    def test_missing_title_key_exits_with_message(self):
        (self.root / 'dist/PPSA99105/sce_sys/param.json').write_text(json.dumps({'titleId': 'PPSA99105'}))
        with self.assertRaisesRegex(SystemExit, 'contentVersion'):
            module.main(self.root)

    def test_reject_changed_backend_inputs(self):
        (self.root / 'src/main.cpp').write_text('changed after backend build')
        with self.assertRaisesRegex(ValueError, 'Backend sources changed'):
            module.package(self.root)

    def test_reject_wrong_model(self):
        (self.root / '.deps/models/ggml-small-q5_1.bin').write_bytes(b'truncated')
        with self.assertRaisesRegex(ValueError, 'Model hash mismatch'):
            module.package(self.root)

    def test_reject_audioin_stub(self):
        (self.root / 'dist/PPSA99105/sce_module/libSceAudioIn.prx').write_bytes(b'link stub')
        with self.assertRaisesRegex(ValueError, 'AudioIn link stub'):
            module.package(self.root)

    def test_reject_stale_title(self):
        (self.root / 'dist/PPSA99105/sce_sys/param.json').write_text(json.dumps({
            'titleId': 'PPSA99105', 'contentVersion': '00.000.002'}))
        with self.assertRaisesRegex(ValueError, 'Native title version is stale'):
            module.package(self.root)


class RemoteSyncTest(unittest.TestCase):
    """Run the build scripts with ssh and rsync redirected to local directories."""

    def sync(self, script):
        real_rsync = shutil.which('rsync')
        self.assertIsNotNone(real_rsync, 'rsync is required by the remote build scripts')
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        base = Path(directory.name)
        local, remote, bin = base / 'local', base / 'remote', base / 'bin'
        for path in (local / 'tools', local / 'src', bin, remote / 'src', remote / '.deps', remote / 'dist'):
            path.mkdir(parents=True)
        shutil.copy2(ROOT / 'tools' / script, local / 'tools' / script)
        (local / 'src/current.cpp').write_text('current')
        (remote / 'src/deleted.cpp').write_text('deleted locally since the last build')
        (remote / '.deps/cache').write_text('cache')
        (remote / 'dist/previous.elf').write_text('previous')
        # Run mkdir for real, stop before the build and the copy back.
        (bin / 'ssh').write_text('#!/bin/sh\nshift\nif [ $# -eq 1 ]; then\n'
                                 '  case "$1" in mkdir*) exec sh -c "$1";; *) exit 3;; esac\nfi\nexec "$@"\n')
        (bin / 'rsync').write_text(f'#!/bin/sh\nexec "{real_rsync}" -e "{bin / "ssh"}" "$@"\n')
        for name in ('ssh', 'rsync'):
            (bin / name).chmod(0o755)
        result = subprocess.run(['bash', str(local / 'tools' / script)], capture_output=True, text=True,
                                env={**os.environ, 'PATH': f'{bin}{os.pathsep}{os.environ["PATH"]}',
                                     'PS5_BUILD_HOST': 'fixture', 'PS5_BUILD_DIRECTORY': str(remote)})
        self.assertEqual(result.returncode, 3, result.stderr)
        self.assertEqual((remote / 'src/current.cpp').read_text(), 'current')
        return remote

    def test_remote_build_inputs_follow_local_deletions(self):
        for script in ('build-backend-remote.sh', 'build-remote.sh'):
            with self.subTest(script=script):
                remote = self.sync(script)
                self.assertFalse((remote / 'src/deleted.cpp').exists())
                self.assertTrue((remote / '.deps/cache').exists())
                self.assertTrue((remote / 'dist/previous.elf').exists())


if __name__ == '__main__':
    unittest.main()
