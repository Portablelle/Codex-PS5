#!/usr/bin/env python3
"""Keep expensive portal builds restricted to main pushes and manual main runs."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PortalWorkflowTest(unittest.TestCase):
    def test_only_main_push_and_manual_dispatch_trigger_full_builds(self):
        workflow = (ROOT / '.github/workflows/portal-release.yml').read_text()
        events = workflow.split('\non:\n', 1)[1].split('\nconcurrency:\n', 1)[0]
        self.assertEqual(events.strip(), 'push:\n    branches: [main]\n  workflow_dispatch:')
        self.assertNotIn('pull_request', events)

    def test_build_and_publish_reject_manual_runs_on_other_branches(self):
        workflow = (ROOT / '.github/workflows/portal-release.yml').read_text()
        jobs = workflow.split('\njobs:\n', 1)[1]
        for name in ('build', 'publish'):
            with self.subTest(job=name):
                job = jobs.split(f'  {name}:\n', 1)[1]
                self.assertEqual(job.splitlines()[0], "    if: github.ref == 'refs/heads/main'")


if __name__ == '__main__':
    unittest.main()
