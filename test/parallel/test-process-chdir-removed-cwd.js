'use strict';

// This tests the error from a failing process.chdir() when the current
// working directory has been removed: the original directory cannot be
// determined, so the error must not report one.
const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const { isMainThread } = require('worker_threads');

if (common.isWindows) {
  common.skip('the current working directory cannot be removed on Windows');
}
if (!isMainThread) {
  common.skip('process.chdir is not available in Workers');
}

const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const dir = tmpdir.resolve('removed-cwd');
fs.mkdirSync(dir);
const missing = tmpdir.resolve('missing');

const child = spawnSync(process.execPath, ['-e', `
  require('fs').rmdirSync(process.cwd());
  try {
    process.chdir(${JSON.stringify(missing)});
  } catch (err) {
    console.log(JSON.stringify({ code: err.code, path: err.path,
                                 dest: err.dest, message: err.message }));
  }
`], { cwd: dir, encoding: 'utf8' });

assert.strictEqual(child.status, 0, child.stderr);
const err = JSON.parse(child.stdout);
assert.strictEqual(err.code, 'ENOENT');
assert.strictEqual(err.path, undefined);
assert.strictEqual(err.dest, missing);
assert.strictEqual(err.message,
                   `ENOENT: no such file or directory, chdir -> '${missing}'`);
