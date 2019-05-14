'use strict';
// Refs: https://github.com/nodejs/node/pull/12022
// If the cwd is deleted, Node cannot run files because the module system
// relies on uv_cwd(). The -e and -p flags still work though.
const common = require('../common');
const assert = require('assert');

if (common.isSunOS || common.isWindows || common.isAIX) {
  // The current working directory cannot be removed on these platforms.
  // Change this to common.skip() when this is no longer a known issue test.
  assert.fail('cannot rmdir current working directory');
}

const cp = require('child_process');
const fs = require('fs');

if (process.argv[2] === 'child') {
  // Do nothing.
} else {
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();
  const dir = fs.mkdtempSync(`${tmpdir.path}/`);
  process.chdir(dir);
  fs.rmdirSync(dir);
  assert.throws(process.cwd,
                /^Error: ENOENT: no such file or directory, uv_cwd$/);

  const r = cp.spawnSync(process.execPath, [__filename, 'child']);

  assert.strictEqual(r.status, 0);
  assert.strictEqual(r.signal, null);
  assert.strictEqual(r.stdout.toString().trim(), '');
  assert.strictEqual(r.stderr.toString().trim(), '');
}
