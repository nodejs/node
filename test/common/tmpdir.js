/* eslint-disable node-core/require-common-first, node-core/required-modules */
'use strict';

const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const { debuglog } = require('util');

const debug = debuglog('test/tmpdir');

function rimrafSync(pathname, { spawn = true } = {}) {
  const st = (() => {
    try {
      return fs.lstatSync(pathname);
    } catch (e) {
      if (fs.existsSync(pathname))
        throw new Error(`Something wonky happened rimrafing ${pathname}`);
      debug(e);
    }
  })();

  // If (!st) then nothing to do.
  if (!st) {
    return;
  }

  // On Windows first try to delegate rmdir to a shell.
  if (spawn && process.platform === 'win32' && st.isDirectory()) {
    try {
      // Try `rmdir` first.
      execSync(`rmdir /q /s ${pathname}`, { timeout: 1000 });
    } catch (e) {
      // Attempt failed. Log and carry on.
      debug(e);
    }
  }

  try {
    if (st.isDirectory())
      rmdirSync(pathname, null);
    else
      fs.unlinkSync(pathname);
  } catch (e) {
    debug(e);
    switch (e.code) {
      case 'ENOENT':
        // It's not there anymore. Work is done. Exiting.
        return;

      case 'EPERM':
        // This can happen, try again with `rmdirSync`.
        break;

      case 'EISDIR':
        // Got 'EISDIR' even after testing `st.isDirectory()`...
        // Try again with `rmdirSync`.
        break;

      default:
        throw e;
    }
    rmdirSync(pathname, e);
  }
}

function rmdirSync(p, originalEr) {
  try {
    fs.rmdirSync(p);
  } catch (e) {
    if (e.code === 'ENOTDIR')
      throw originalEr;
    if (e.code === 'ENOTEMPTY' || e.code === 'EEXIST' || e.code === 'EPERM') {
      const enc = process.platform === 'linux' ? 'buffer' : 'utf8';
      fs.readdirSync(p, enc).forEach((f) => {
        if (f instanceof Buffer) {
          const buf = Buffer.concat([Buffer.from(p), Buffer.from(path.sep), f]);
          rimrafSync(buf);
        } else {
          rimrafSync(path.join(p, f));
        }
      });
      fs.rmdirSync(p);
    }
  }
}

const testRoot = process.env.NODE_TEST_DIR ?
  fs.realpathSync(process.env.NODE_TEST_DIR) : path.resolve(__dirname, '..');

// Using a `.` prefixed name, which is the convention for "hidden" on POSIX,
// gets tools to ignore it by default or by simple rules, especially eslint.
const tmpdirName = '.tmp.' + (process.env.TEST_THREAD_ID || '0');
const tmpPath = path.join(testRoot, tmpdirName);

function refresh(opts = {}) {
  rimrafSync(this.path, opts);
  fs.mkdirSync(this.path);
}

module.exports = {
  path: tmpPath,
  refresh
};
