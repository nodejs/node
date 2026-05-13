---
name: conditional_testing
description: Documentation for conditional testing in V8 using gm.py.
---

# Skill: Conditional Testing

This skill documents how to use conditional testing to optimize the testing
workflow in V8.

## Purpose

Running the full V8 test suite can be time-consuming. Conditional testing saves
time and resources by checking if tests are actually needed. It will build
and/or run tests only if:

1. `d8` executable is missing or out of date relative to modified source files
   (detected via `git diff` and `git ls-files`).
2. The `d8` executable is newer than the last successful test run.
3. Any test file has been modified since the last successful test run.

## Usage

### Default Behavior

For regular users, conditional testing is **enabled by default** when executing
a full test suite (e.g. `check`).

```bash
gm.py x64.release.check
```

If nothing has changed, it will silently exit without rebuilding or re-running
tests. If only test files changed, it will re-run the tests without building. If
you explicitly request a subset of tests (e.g.,
`gm.py x64.release mjsunit/foo`), the tests will run unconditionally.

### Agent Workflow

For automated agents, you can use the `--ensure-tests-ran` flag without
specifying a target configuration. This will automatically detect the most
recently built configuration in `out/` and run the tests (if needed) for that
specific configuration.

```bash
tools/dev/gm.py --ensure-tests-ran
```

## How It Works

1. **Checks Modified Files**: Uses `git diff origin/main` and
   `git ls-files --others` to efficiently find files modified in the worktree.
2. **Checks Build Need**: If any C++, GN, or Bazel files (excluding `test/` and
   `tools/` directories) are modified since `d8` was last built, a build is
   triggered.
3. **Checks Timestamps**: Compares the modification time of `d8` with a cached
   timestamp in `<outdir>/last_test_run`. If `d8` is newer, tests are run.
4. **Checks Test Need**: If any file in the `test/` directory is newer than
   `last_test_run`, tests are run.
5. **Updates Timestamp**: On a successful full test run, it updates the
   `<outdir>/last_test_run` file timestamp.

## Best Practices

- Always rely on `gm.py --ensure-tests-ran` for background checking.
- If you want to force a full test run, you can either touch `d8` or remove the
  `last_test_run` file.
