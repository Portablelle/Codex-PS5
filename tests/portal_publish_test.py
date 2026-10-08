#!/usr/bin/env python3
"""Exercise the workflow publication script against a local GitHub CLI fixture."""
import os
from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[1]
GH = '''#!/usr/bin/env python3
import json, os, pathlib, sys
args = sys.argv[1:]
scenario = os.environ['SCENARIO']
with open('calls.txt', 'a') as stream:
    stream.write(' '.join(args) + '\\n')
if '--slurp' in args:
    print('unknown flag: --slurp', file=sys.stderr)
    sys.exit(1)
if args[0] == 'api':
    if scenario == 'unauthorized':
        print('HTTP 401', file=sys.stderr)
        sys.exit(1)
    if '/git/ref/tags/' in args[1]:
        if scenario == 'new' or scenario.startswith('draft') or scenario == 'published_no_tag':
            print('HTTP 404', file=sys.stderr)
            sys.exit(1)
        print('commit ' + ('b' * 40 if scenario == 'wrong_tag' else os.environ['COMMIT']))
    elif '/releases?' in args[1]:
        if scenario == 'new':
            print('[]')
            sys.exit(0)
        draft = scenario.startswith('draft')
        assets = [] if scenario.endswith('missing') else [{'name': 'codex-portal.tar.gz'}]
        target = 'b' * 40 if scenario == 'draft_wrong_target' else os.environ['COMMIT']
        release = {'tag_name': 'portal-' + os.environ['COMMIT'], 'draft': draft,
                   'target_commitish': target, 'assets': assets}
        print(json.dumps([{'tag_name': 'unrelated-release', 'draft': False, 'assets': []}]))
        print(json.dumps([release]))
        if scenario == 'draft_duplicate':
            print(json.dumps([release]))
    else:
        sys.exit(2)
elif args[0:2] == ['release', 'download']:
    target = pathlib.Path(args[args.index('--dir') + 1]) / 'codex-portal.tar.gz'
    content = b'different' if scenario.endswith('mismatch') else pathlib.Path('codex-portal.tar.gz').read_bytes()
    target.write_bytes(content)
elif args[0] == 'release' and args[1] in ('create', 'upload', 'edit'):
    if '--clobber' in args:
        sys.exit(2)
else:
    sys.exit(2)
'''


class PortalPublishTest(unittest.TestCase):
    def publish(self, scenario):
        workflow = (ROOT / '.github/workflows/portal-release.yml').read_text()
        script = textwrap.dedent(workflow.rsplit('        run: |\n', 1)[1])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            gh = root / 'gh'
            gh.write_text(GH)
            gh.chmod(0o755)
            (root / 'codex-portal.tar.gz').write_bytes(b'package fixture')
            (root / 'existing').mkdir()
            (root / 'existing/codex-portal.tar.gz').write_bytes(b'stale compare fixture')
            env = dict(os.environ, PATH=str(root) + os.pathsep + os.environ['PATH'],
                       SCENARIO=scenario, COMMIT='a' * 40, GH_REPO='fixture/repo')
            result = subprocess.run(['bash', '-c', script], cwd=root, env=env,
                                    capture_output=True, text=True)
            return result.returncode, (root / 'calls.txt').read_text()

    def test_new_release_is_published_after_upload(self):
        code, calls = self.publish('new')
        self.assertEqual(code, 0)
        self.assertLess(calls.index('release create'), calls.index('release upload'))
        self.assertLess(calls.index('release upload'), calls.index('release edit'))
        self.assertIn('--draft --prerelease', calls)
        self.assertIn('--latest=false', calls)

    def test_draft_without_asset_is_completed(self):
        code, calls = self.publish('draft_missing')
        self.assertEqual(code, 0)
        self.assertNotIn('release create', calls)
        self.assertIn('release upload', calls)
        self.assertIn('release edit', calls)

    def test_draft_with_matching_asset_is_published_without_overwrite(self):
        code, calls = self.publish('draft_matching')
        self.assertEqual(code, 0)
        self.assertIn('release download', calls)
        self.assertIn('release edit', calls)
        self.assertNotIn('release upload', calls)

    def test_published_matching_asset_is_untouched(self):
        code, calls = self.publish('published_matching')
        self.assertEqual(code, 0)
        self.assertNotIn('release edit', calls)
        self.assertNotIn('release upload', calls)

    def test_mismatching_assets_missing_published_asset_and_bad_tag_fail_closed(self):
        for scenario in ('draft_mismatch', 'published_mismatch', 'published_missing', 'wrong_tag', 'unauthorized',
                         'draft_wrong_target', 'draft_duplicate', 'published_no_tag'):
            with self.subTest(scenario=scenario):
                code, calls = self.publish(scenario)
                self.assertNotEqual(code, 0)
                self.assertNotIn('release upload', calls)
                self.assertNotIn('release create', calls)
                self.assertNotIn('release edit', calls)


if __name__ == '__main__':
    unittest.main()
