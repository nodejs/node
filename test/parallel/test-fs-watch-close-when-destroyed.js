'use strict';

// This tests that closing a watcher when the underlying handle is
// already destroyed will result in a noop instead of a crash.

const common = require('../common');

// fs-watch on folders have limited capability in AIX.
// The testcase makes use of folder watching, and causes
// hang. This behavior is documented. Skip this for AIX.

if (common.isAIX)
  common.skip('folder watch capability is limited in AIX.');

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

const tmpdir = require('../common/tmpdir');
const fs = require('fs');

tmpdir.refresh();
const root = tmpdir.resolve('watched-directory');
fs.mkdirSync(root);

const watcher = fs.watch(root, { persistent: false, recursive: false });

// The following listeners may or may not be invoked.

watcher.addListener('error', () => {
  setTimeout(
    () => { watcher.close(); },  // Should not crash if it's invoked
    common.platformTimeout(10)
  );
});

watcher.addListener('change', () => {
  setTimeout(
    () => { watcher.close(); },
    common.platformTimeout(10)
  );
});

fs.rmdirSync(root);
// Wait for the listener to hit
setTimeout(
  common.mustCall(),
  common.platformTimeout(100)
);
