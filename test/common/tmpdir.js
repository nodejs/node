/* eslint-disable node-core/require-common-first, node-core/required-modules */
'use strict';

const fs = require('fs');
const path = require('path');
const { isMainThread } = require('worker_threads');

function rimrafSync(pathname) {
  fs.rmdirSync(pathname, { maxRetries: 3, recursive: true });
}

const testRoot = process.env.NODE_TEST_DIR ?
  fs.realpathSync(process.env.NODE_TEST_DIR) : path.resolve(__dirname, '..');

// Using a `.` prefixed name, which is the convention for "hidden" on POSIX,
// gets tools to ignore it by default or by simple rules, especially eslint.
const tmpdirName = '.tmp.' +
  (process.env.TEST_SERIAL_ID || process.env.TEST_THREAD_ID || '0');
const tmpPath = path.join(testRoot, tmpdirName);

let firstRefresh = true;
function refresh() {
  rimrafSync(this.path);
  fs.mkdirSync(this.path);

  if (firstRefresh) {
    firstRefresh = false;
    // Clean only when a test uses refresh. This allows for child processes to
    // use the tmpdir and only the parent will clean on exit.
    process.on('exit', onexit);
  }
}

function onexit() {
  // Change directory to avoid possible EBUSY
  if (isMainThread)
    process.chdir(testRoot);

  try {
    rimrafSync(tmpPath);
  } catch (e) {
    console.error('Can\'t clean tmpdir:', tmpPath);

    const files = fs.readdirSync(tmpPath);
    console.error('Files blocking:', files);

    if (files.some((f) => f.startsWith('.nfs'))) {
      // Warn about NFS "silly rename"
      console.error('Note: ".nfs*" might be files that were open and ' +
                    'unlinked but not closed.');
      console.error('See http://nfs.sourceforge.net/#faq_d2 for details.');
    }

    console.error();
    throw e;
  }
}

module.exports = {
  path: tmpPath,
  refresh
};
