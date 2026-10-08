#!/usr/bin/env python3
"""Keep pinned loader patching independent of any ancestor Git checkout."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = b'original loader\n'
PATCHED = b'patched loader\n'
PATCH = b'''diff --git a/elfldr.c b/elfldr.c
--- a/elfldr.c
+++ b/elfldr.c
@@ -1 +1 @@
-original loader
+patched loader
'''


class LoaderFetchTest(unittest.TestCase):
    def prepare(self, root):
        for name in ('tools', 'patches', 'vendor/shsrv'):
            (root / name).mkdir(parents=True)
        shutil.copy2(ROOT / 'vendor/ps5-ai-cli/tools/fetch-loader.py', root / 'tools/fetch-loader.py')
        (root / 'patches/loader.patch').write_bytes(PATCH)
        (root / 'vendor/shsrv/elfldr.c').write_bytes(ORIGINAL)
        (root / 'sources.lock.json').write_text(json.dumps({'loader': {
            'commit': 'a' * 40,
            'files': {'elfldr.c': hashlib.sha256(ORIGINAL).hexdigest()},
            'patched_files': {'elfldr.c': hashlib.sha256(PATCHED).hexdigest()},
            'patch': {'path': 'patches/loader.patch', 'sha256': hashlib.sha256(PATCH).hexdigest()},
        }}))

    def test_standalone_and_nested_git_stage_apply_exact_patch_and_repeat(self):
        for nested in (False, True):
            with self.subTest(nested=nested), tempfile.TemporaryDirectory() as directory:
                parent = Path(directory)
                root = parent / '.deps/backend' if nested else parent / 'backend'
                if nested:
                    subprocess.run(['git', 'init', '-q', str(parent)], check=True)
                    (parent / 'vendor/shsrv').mkdir(parents=True)
                    (parent / 'vendor/shsrv/elfldr.c').write_bytes(ORIGINAL)
                self.prepare(root)
                command = ['python3', str(root / 'tools/fetch-loader.py')]
                for arguments in ([], [], ['--offline']):
                    result = subprocess.run(command + arguments, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual((root / 'vendor/shsrv/elfldr.c').read_bytes(), PATCHED)
                if nested:
                    self.assertEqual((parent / 'vendor/shsrv/elfldr.c').read_bytes(), ORIGINAL)

    def test_reject_changed_patch_and_changed_loader_without_overwriting(self):
        for name in ('patches/loader.patch', 'vendor/shsrv/elfldr.c'):
            with self.subTest(path=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory) / 'backend'
                self.prepare(root)
                (root / name).write_bytes(b'unexpected content\n')
                before = (root / 'vendor/shsrv/elfldr.c').read_bytes()
                result = subprocess.run(['python3', str(root / 'tools/fetch-loader.py')],
                                        capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual((root / 'vendor/shsrv/elfldr.c').read_bytes(), before)

    def test_checksum_pinned_patch_cannot_escape_loader_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / 'backend'
            self.prepare(root)
            outside = root / 'outside.c'
            outside.write_bytes(ORIGINAL)
            unsafe = PATCH.replace(b'elfldr.c', b'../../outside.c')
            (root / 'patches/loader.patch').write_bytes(unsafe)
            pin = json.loads((root / 'sources.lock.json').read_text())
            pin['loader']['patch']['sha256'] = hashlib.sha256(unsafe).hexdigest()
            (root / 'sources.lock.json').write_text(json.dumps(pin))
            result = subprocess.run(['python3', str(root / 'tools/fetch-loader.py')],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(outside.read_bytes(), ORIGINAL)
            self.assertEqual((root / 'vendor/shsrv/elfldr.c').read_bytes(), ORIGINAL)


if __name__ == '__main__':
    unittest.main()
