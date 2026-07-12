'use strict';

require('../common');

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

fs.watchFile(missing, { interval: -0 }, () => {});
fs.unwatchFile(missing);
