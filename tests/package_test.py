#!/usr/bin/env python3
"""Exercise public archive layout, repeat packaging, and release input rejection."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('package', Path(__file__).resolve().parents[1] / 'tools/package.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PackageTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        for path in ('dist/PPSA99105/sce_sys', 'dist/PPSA99105/assets', 'dist/PPSA99105/sce_module',
                     '.deps/models', 'licenses', 'vendor/ps5-ai-cli/LICENSES', 'src', 'backend', 'tools'):
            (self.root / path).mkdir(parents=True)
        for name in ('README.md', 'INSTALL.md', 'THIRD_PARTY.md', 'LICENSE',
                     'licenses/native.txt', 'vendor/ps5-ai-cli/LICENSES/engine.txt'):
            (self.root / name).write_text('fixture notice\n')
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


if __name__ == '__main__':
    unittest.main()
