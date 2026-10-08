'use strict';

require('../common');

const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

const missing = path.join(
  os.tmpdir(),
  `node-fs-negative-zero-${process.pid}`,
  'entry',
);

function ignoreExpectedError(fn) {
  try {
    fn();
  } catch {
    // Ignore expected file system errors from the missing path.
  }
}

const fd = fs.openSync(process.execPath, -0);
fs.closeSync(fd);

ignoreExpectedError(() => fs.openSync(process.execPath, 'r', -0));
ignoreExpectedError(() => fs.readFileSync(process.execPath, { flag: -0 }));
ignoreExpectedError(() => fs.mkdirSync(missing, { mode: -0 }));
ignoreExpectedError(() => fs.chmodSync(missing, -0));
ignoreExpectedError(() => fs.writeFileSync(missing, '', { mode: -0 }));

// -0 is accepted as file descriptor 0. Writing an empty string reaches the
// utf8 fast path without issuing a write on the descriptor.
fs.writeFileSync(-0, '');
fs.appendFileSync(-0, '');

const child = spawnSync(
  process.execPath,
  ['-e', 'process.stdout.write(require("fs").readFileSync(-0, "utf8"))'],
  { input: 'hello' },
);
assert.strictEqual(child.status, 0);
assert.strictEqual(child.stdout.toString(), 'hello');

fs.watchFile(missing, { interval: -0 }, () => {});
fs.unwatchFile(missing);
