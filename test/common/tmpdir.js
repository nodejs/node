/* eslint-disable node-core/require-common-first, node-core/required-modules */
'use strict';

const fs = require('fs');
const path = require('path');

function rimrafSync(pathname) {
  fs.rmdirSync(pathname, { maxRetries: 3, recursive: true });
}

const testRoot = process.env.NODE_TEST_DIR ?
  fs.realpathSync(process.env.NODE_TEST_DIR) : path.resolve(__dirname, '..');

// Using a `.` prefixed name, which is the convention for "hidden" on POSIX,
// gets tools to ignore it by default or by simple rules, especially eslint.
const tmpdirName = '.tmp.' + (process.env.TEST_THREAD_ID || '0');
const tmpPath = path.join(testRoot, tmpdirName);

function refresh() {
  rimrafSync(this.path);
  fs.mkdirSync(this.path);
}

module.exports = {
  path: tmpPath,
  refresh
};
