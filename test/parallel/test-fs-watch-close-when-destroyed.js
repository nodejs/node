'use strict';

// This tests that closing a watcher when the underlying handle is
// already destroyed will result in a noop instead of a crash.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();
const root = path.join(tmpdir.path, 'watched-directory');
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
  common.mustCall(() => {}),
  common.platformTimeout(100)
);
