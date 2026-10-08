#!/usr/bin/env python3
"""Exercise commit provenance, standalone export, hashes and source isolation."""
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import unittest
from unittest.mock import patch

import package_test

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
spec = importlib.util.spec_from_file_location('portal_package', ROOT / 'tools/package-portal.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PortalPackageTest(unittest.TestCase):
    def setUp(self):
        package_test.PackageTest.setUp(self)
        shutil.copy2(ROOT / 'tools/export-portal.py', self.root / 'tools/export-portal.py')
        (self.root / '.gitignore').write_text('dist/\n.deps/\n.env\nbackups/\n')
        (self.root / '.env').write_text('private fixture, never archive')
        (self.root / 'backups').mkdir()
        (self.root / 'backups/console.log').write_text('private fixture log')
        self.commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        self.git_status = b''
        real_output, real_run = subprocess.check_output, subprocess.run

        def git_output(command, **kwargs):
            if command == ['git', 'rev-parse', 'HEAD'] and kwargs.get('cwd') == self.root:
                return self.commit
            if command[0:2] == ['git', 'status'] and kwargs.get('cwd') == self.root:
                return self.git_status
            return real_output(command, **kwargs)

        def git_archive(command, **kwargs):
            if command[0:2] == ['git', 'archive']:
                kwargs['cwd'] = ROOT
            return real_run(command, **kwargs)

        self.addCleanup(patch.stopall)
        patch.object(module.subprocess, 'check_output', side_effect=git_output).start()
        patch.object(module.subprocess, 'run', side_effect=git_archive).start()
        inputs = [self.root / 'VERSION', self.root / 'upstream-lock.json',
                  *sorted((self.root / 'backend').glob('*')), *sorted((self.root / 'tools').glob('*')),
                  *sorted((self.root / 'src').glob('*'))]
        identity = hashlib.sha256(b''.join(p.relative_to(self.root).as_posix().encode() + b'\0' + p.read_bytes()
                                           for p in inputs if p.is_file())).hexdigest()
        (self.root / 'dist/service-build.json').write_text(json.dumps({'version': '0.0.3', 'build': identity}))

    def test_complete_hashes_committed_source_and_repeatability(self):
        output = module.package_portal(self.root)
        first = output.read_bytes()
        with tarfile.open(output) as archive:
            files = {entry.name: archive.extractfile(entry).read() for entry in archive.getmembers()}
        metadata = json.loads(files.pop('codex-release.json'))
        self.assertEqual(metadata['schema'], 1)
        self.assertEqual(metadata['version'], '0.0.3')
        self.assertEqual(metadata['commit'], self.commit)
        self.assertEqual(metadata['sha256'], {name: hashlib.sha256(content).hexdigest()
                                             for name, content in files.items()})
        self.assertIn('src/codex-payload.js', files)
        for name in ('manifest.json', 'LICENSE', 'NOTICE.md', 'codex-source.tar.gz'):
            self.assertIn('apps/codex/' + name, files)
        self.assertNotIn('src/codex-install.js', files)
        with tarfile.open(fileobj=io.BytesIO(files['apps/codex/codex-source.tar.gz'])) as source:
            names = source.getnames()
            self.assertIn('codex-source/src/main.cpp', names)
            self.assertIn('codex-source/vendor/ps5-ai-cli/LICENSE', names)
            self.assertFalse(any('/.deps/' in name or '/dist/' in name or '/backups/' in name
                                 or name.endswith('/.env') or '/.git/' in name for name in names))
        manifest = json.loads(files['apps/codex/manifest.json'])
        for chunk in manifest['files']:
            block = files['apps/codex/' + chunk['path']]
            self.assertEqual(len(block), chunk['size'])
            self.assertEqual(hashlib.sha256(block).hexdigest(), chunk['sha256'])
        for logical in manifest['native'] + manifest['service']:
            content = b''.join(files['apps/codex/chunks/' + chunk + '.bin'] for chunk in logical['chunks'])
            self.assertEqual(len(content), logical['size'])
            self.assertEqual(hashlib.sha256(content).hexdigest(), logical['sha256'])
        payload = json.loads(files['src/codex-payload.js'].decode().removeprefix('export const PAYLOAD = ').strip().removesuffix(';'))
        service = dict(manifest['service'][0])
        service.pop('path')
        service['chunkSize'] = manifest['chunkSize']
        self.assertEqual(payload, service)
        self.assertNotIn('codex-source.tar.gz', [entry['path'] for entry in manifest['files']])
        self.assertEqual(module.package_portal(self.root).read_bytes(), first)

    def test_reject_uncommitted_source(self):
        (self.root / 'src/main.cpp').write_text('changed')
        self.git_status = b' M src/main.cpp\n'
        with self.assertRaisesRegex(ValueError, 'Commit all source changes'):
            module.package_portal(self.root)

    def test_reject_untracked_source(self):
        (self.root / 'src/new.cpp').write_text('not committed')
        self.git_status = b'?? src/new.cpp\n'
        with self.assertRaisesRegex(ValueError, 'Commit all source changes'):
            module.package_portal(self.root)

    def test_reject_stale_artifacts(self):
        (self.root / 'src/main.cpp').write_text('changed')
        with self.assertRaisesRegex(ValueError, 'Backend sources changed'):
            module.package_portal(self.root)

    def test_export_updates_existing_portal_installer(self):
        module.package(self.root)
        portal = self.root / '.deps/portal'
        (portal / 'src').mkdir(parents=True)
        installer = portal / 'src/codex-install.js'
        installer.write_text("const HASH = '" + '0' * 64 + "';\n")
        subprocess.run(['python3', str(self.root / 'tools/export-portal.py'),
                        '--portal-root', str(portal)], check=True)
        digest = hashlib.sha256((portal / 'apps/codex/manifest.json').read_bytes()).hexdigest()
        self.assertEqual(installer.read_text(), f"const HASH = '{digest}';\n")


if __name__ == '__main__':
    unittest.main()
