/* eslint-disable node-core/required-modules */
'use strict';

const fs = require('fs');
const path = require('path');

function rimrafSync(p) {
  let st;
  try {
    st = fs.lstatSync(p);
  } catch (e) {
    if (e.code === 'ENOENT')
      return;
  }

  try {
    if (st && st.isDirectory())
      rmdirSync(p, null);
    else
      fs.unlinkSync(p);
  } catch (e) {
    if (e.code === 'ENOENT')
      return;
    if (e.code === 'EPERM')
      return rmdirSync(p, e);
    if (e.code !== 'EISDIR')
      throw e;
    rmdirSync(p, e);
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
let tmpdirName = '.tmp';
if (process.env.TEST_THREAD_ID) {
  tmpdirName += `.${process.env.TEST_THREAD_ID}`;
}

const tmpPath = path.join(testRoot, tmpdirName);

function refresh() {
  rimrafSync(this.path);
  fs.mkdirSync(this.path);
}

module.exports = {
  path: tmpPath,
  refresh
};
