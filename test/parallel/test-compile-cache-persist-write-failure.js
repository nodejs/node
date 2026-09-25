'use strict';

// Regression test for https://github.com/nodejs/node/issues/65473
//
// When CompileCacheHandler::Persist() fails to write, close, or rename the
// temporary cache file (e.g. because the underlying filesystem operation
// fails with ENOSPC/EDQUOT/EFBIG), it used to `continue` without closing the
// already-open descriptor or removing the already-created temporary file.
// A persistent failure could therefore leak an open file descriptor and a
// zero-byte temporary file for every module compiled, for the lifetime of
// the process.
//
// This uses `ulimit -f 0` to make uv_fs_mkstemp() succeed (creating an empty,
// 0-byte file is allowed) while the subsequent uv_fs_write() fails with
// EFBIG, reliably reproducing a failure partway through persistence without
// needing to fill up a real filesystem.

const common = require('../common');

if (common.isWindows)
  common.skip('no RLIMIT_FSIZE on Windows');

if (process.config.variables.node_shared)
  common.skip('SIGXFSZ signal handler not installed in shared library mode');

const assert = require('assert');
const child_process = require('child_process');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

function listFilesRecursive(dir) {
  const result = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      result.push(...listFilesRecursive(full));
    } else {
      result.push(full);
    }
  }
  return result;
}

tmpdir.refresh();
const cacheDir = tmpdir.resolve('.compile_cache_dir');

const [cmd, opts] = common.escapePOSIXShell`ulimit -f 0 && "${process.execPath}" "${fixtures.path('empty.js')}"`;
const result = child_process.spawnSync('/bin/sh', ['-c', cmd], {
  ...opts,
  env: {
    ...opts.env,
    NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
    NODE_COMPILE_CACHE: cacheDir,
  },
  cwd: tmpdir.path,
});

const stderr = result.stderr.toString();
const stdout = result.stdout.toString();

assert.strictEqual(result.status, 0, `child should exit cleanly, got status ${result.status}\nstderr: ${stderr}`);

// Sanity check: the failure path this test targets was actually exercised.
// If this does not match, `ulimit -f 0` did not make persistence fail the
// way this test expects, and the test would pass vacuously.
assert.match(
  stderr,
  /writing cache for .*empty\.js.*failed: /,
  `expected a failed persistence attempt in stderr, got:\n${stderr}\n---\n${stdout}`);

// The actual regression check: nothing should be left behind in the compile
// cache directory once the failed child process has exited, no matter how
// persistence failed.
const leftover = fs.existsSync(cacheDir) ? listFilesRecursive(cacheDir) : [];
assert.deepStrictEqual(
  leftover,
  [],
  `expected no leftover files in the compile cache directory, found:\n${leftover.join('\n')}`);
