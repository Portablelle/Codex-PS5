---
description: Review a pull request for correctness (used by the review workflow; run locally as /review-pr <number>)
argument-hint: <pr-number>
---

Review pull request #$ARGUMENTS of this repository for correctness. Do the work
yourself: do not spawn sub-agents and do not wait for background tasks. Never
commit, push, or edit files in the checkout (temporary reverts to run a test are
fine; restore them before you finish). Read `CLAUDE.md` first: it lists the build
and test commands, the target platforms, and the invariants to check.

**Where to post.** When the environment variable `GITHUB_ACTIONS` is `true`, post
on GitHub as described below. Otherwise you are running locally: post nothing,
and print the summary and findings in the terminal instead.

## 1. Gather context

- `gh pr view $ARGUMENTS --json title,body,author,baseRefName,headRefOid` (always
  with `--json`: plain `gh pr view` fails on CI-status permissions).
- `gh pr diff $ARGUMENTS --name-only` for the full file list, then the diff itself
  (`git diff origin/<base>...HEAD`). Output can be truncated: read every changed
  file in the checkout, in full when the diff was cut off. Do not skip tests,
  scripts or docs.
- Earlier review state:
  - the previous summary, which is the issue comment containing
    `<!-- claude-review` (`gh api repos/{owner}/{repo}/issues/$ARGUMENTS/comments --paginate`);
  - the inline review comments and their replies
    (`gh api repos/{owner}/{repo}/pulls/$ARGUMENTS/comments --paginate`).
  The marker holds the last reviewed SHA: `<!-- claude-review sha=<sha> -->`.

**Incremental review.** If a previous summary exists and its SHA is an ancestor of
HEAD (`git merge-base --is-ancestor <sha> HEAD`), focus on `git diff <sha>..HEAD`.
Still read enough of the rest of the PR to judge the new commits in context.
Otherwise (first review or force-push), review the whole PR.

Do not raise again a point that was already reported, answered or declined in an
earlier comment or thread. Carry it over with its current status instead.

## 2. Run the tests

Run `bash tools/test.sh` and record the result (exit code and the failing check, if
any). When the PR fixes a bug and adds a regression test, check that the test
actually proves the fix:

1. Revert only the fixed source files to the base (`git checkout origin/<base> -- <files>`).
2. Rerun the test and confirm it fails.
3. Restore the files (`git checkout HEAD -- <files>`).

macOS results come from the `Tests` workflow
(`gh run list --commit <head sha> --workflow tests.yml --json conclusion,status,url`).
If it has not finished, say it is pending. Do not wait for it.

## 3. Look for real problems

Report only problems you are confident about, after checking each one against the
surrounding code. Focus on:

- **Correctness:**
  - bugs and crashes;
  - wrong logic and broken edge cases;
  - missing error handling;
  - races and resource or descriptor leaks;
  - security issues.
- **Invariants and platform differences** listed in `CLAUDE.md`:
  - PS5, macOS and Linux behavior;
  - warnings under `-Werror`.
- **Tests:**
  - Is risky new logic covered?
  - Does each test assert what its name claims?
  - Does a regression test fail without the fix?
  - Is a test timing-dependent or flaky?
- **Docs vs code:** `README.md` and `INGAME.md` must still describe what the code does.

Skip style nitpicks and anything a compiler or linter would catch.

Rate each finding:

- 🔴 **blocking**: wrong behavior, data loss, security, or broken tests;
- 🟡 **should fix**: real but limited impact, or a missing test for risky logic;
- ⚪ **note**: worth knowing, no change required.

## 4. Post inline comments

Post each 🔴 and 🟡 finding as an inline comment on the exact line, with
`mcp__github_inline_comment__create_inline_comment` and `confirmed: true`. Start
each comment with its severity and an ID (`R1`, `R2`, …, continuing the numbering of
earlier reviews). Then state the problem, a concrete failure scenario and a suggested
fix. ⚪ notes go only in the summary.

## 5. Write the summary

Keep one summary comment per PR. Write the body to a temporary file outside the
checkout, then:

- if a previous summary exists, update it:
  `gh api -X PATCH repos/{owner}/{repo}/issues/comments/<id> -F body=@<file>`;
- otherwise create it: `gh pr comment $ARGUMENTS --body-file <file>`.

Format (concise, no filler):

```
<!-- claude-review sha=<head sha> -->
## Claude review: <🔴 changes needed | 🟡 should fix before merge | ✅ no issues found>

Reviewed <full PR | commits <old sha7>..<new sha7>> · tests: <Linux pass/fail · macOS pass/fail/pending>

| ID | Severity | Location | Finding | Status |
|----|----------|----------|---------|--------|
| R1 | 🟡 | `path:line` | one line | open / fixed in <sha7> / declined |

**Notes:** ⚪ items, one line each (omit the section if empty).

**Not verified:** only the things that matter for this PR (e.g. PS5 build, console behavior).

<details><summary>Files reviewed</summary>
...list, and any file you could not review, with the reason...
</details>
```

The table lists every finding from this review and from earlier ones. Mark a
finding `fixed` only after checking the new code. The verdict reflects the open
findings: any open 🔴 means "changes needed".

Write all comments in English.
