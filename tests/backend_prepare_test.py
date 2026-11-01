#!/usr/bin/env python3
"""Verify direct runner staging without touching vendored sources or real artifacts."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BackendPrepareTest(unittest.TestCase):
    def test_direct_staging_is_repeatable_and_preserves_docker_mode(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('backend', 'src', 'tools', 'vendor/ps5-ai-cli'):
                shutil.copytree(ROOT / name, root / name, ignore=shutil.ignore_patterns('__pycache__'))
            for name in ('VERSION', 'upstream-lock.json'):
                shutil.copy2(ROOT / name, root / name)
            original = (root / 'vendor/ps5-ai-cli/tools/check-codex.sh').read_bytes()
            subprocess.run(['python3', str(root / 'tools/prepare-backend.py'), '--direct'], check=True)
            staged = root / '.deps/backend'
            tools = {p.name: p.read_bytes() for p in (staged / 'tools').iterdir()
                     if p.is_file() and p.suffix in ('.sh', '.py')}
            self.assertIn(str(staged).encode(), tools['check-codex.sh'])
            self.assertFalse(any(b'/work' in content for content in tools.values()))
            self.assertIn(b'--max-size-mib 256', tools['check-codex.sh'])
            self.assertIn(b'assistant-service; do', tools['ps5-rust-linker.sh'])
            self.assertIn(b'reallocarray=sbase_reallocarray', tools['build-shell.sh'])
            self.assertEqual((root / 'vendor/ps5-ai-cli/tools/check-codex.sh').read_bytes(), original)
            inputs = [root / 'VERSION', root / 'upstream-lock.json',
                      *sorted((root / 'backend').glob('*')), *sorted((root / 'tools').glob('*')),
                      *sorted((root / 'src').glob('*'))]
            identity = hashlib.sha256(b''.join(p.relative_to(root).as_posix().encode() + b'\0' + p.read_bytes()
                                               for p in inputs if p.is_file())).hexdigest()
            self.assertEqual(json.loads((root / 'dist/service-build.json').read_text())['build'], identity)
            subprocess.run(['python3', str(root / 'tools/prepare-backend.py'), '--direct'], check=True)
            self.assertEqual(tools, {name: (staged / 'tools' / name).read_bytes() for name in tools})
            subprocess.run(['python3', str(root / 'tools/prepare-backend.py')], check=True)
            self.assertIn(b'/work', (staged / 'tools/check-codex.sh').read_bytes())

    def test_codex_session_path_falls_back_to_utc(self):
        # Issue 36: localtime_r can fail on the PS5, which made every thread/start fail.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('backend', 'src', 'tools', 'vendor/ps5-ai-cli'):
                shutil.copytree(ROOT / name, root / name, ignore=shutil.ignore_patterns('__pycache__'))
            for name in ('VERSION', 'upstream-lock.json'):
                shutil.copy2(ROOT / name, root / name)
            subprocess.run(['python3', str(root / 'tools/prepare-backend.py'), '--direct'], check=True,
                           stdout=subprocess.DEVNULL)
            patches = sorted((root / '.deps/backend/patches/codex').glob('*.patch'))
            self.assertEqual(patches[-1].name, '0100-ps5-local-time-fallback.patch')
            # The function as pinned in Codex rust-v0.160.0 (codex-rs/rollout/src/recorder.rs).
            codex = root / 'codex'
            source = codex / 'codex-rs/rollout/src/recorder.rs'
            source.parent.mkdir(parents=True)
            source.write_text('fn precompute_new_rollout_path(\n'
                              '    config: &impl RolloutConfigView,\n'
                              '    thread_id: ThreadId,\n'
                              '    rollout_id_override: Option<RolloutId>,\n'
                              ') -> std::io::Result<(PathBuf, OffsetDateTime)> {\n'
                              '    // Resolve ~/.codex/sessions/YYYY/MM/DD path.\n'
                              '    let timestamp = OffsetDateTime::now_local()\n'
                              '        .map_err(|e| IoError::other(format!("failed to get local time: {e}")))?;\n'
                              '    let mut dir = config.codex_home().to_path_buf();\n'
                              '    dir.push(SESSIONS_SUBDIR);\n'
                              '    dir.push(timestamp.year().to_string());\n'
                              '}\n')
            subprocess.run(['git', 'init', '-q', str(codex)], check=True)
            subprocess.run(['git', 'apply', str(patches[-1])], cwd=codex, check=True)
            patched = source.read_text()
            self.assertIn('now_local().unwrap_or_else(|_| OffsetDateTime::now_utc())', patched)
            self.assertNotIn('failed to get local time', patched)


if __name__ == '__main__':
    unittest.main()
