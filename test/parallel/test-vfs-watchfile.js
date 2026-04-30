'use strict';

// Tests for VFS watchFile/unwatchFile:
// - unwatchFile(path) without a specific listener cleans up properly
// - watchFile() zero stats for missing file use all-zero mode

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// unwatchFile(path) without a specific listener must clean up the timer.
// If the fix is wrong, the process would hang due to a leaked timer.
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'x');

  myVfs.watchFile('/a.txt', { interval: 50, persistent: false }, () => {});
  myVfs.unwatchFile('/a.txt');
}

// watchFile() zero stats for a missing file must have all-zero mode.
// The previous-stats argument for a newly-created file should report
// isFile() === false and mode === 0.
{
  const myVfs = vfs.create();

  function listener(curr, prev) {
    assert.strictEqual(prev.isFile(), false);
    assert.strictEqual(prev.mode, 0);
    myVfs.unwatchFile('/missing.txt', listener);
  }

  myVfs.watchFile('/missing.txt', { interval: 50, persistent: false }, listener);
  setTimeout(() => myVfs.writeFileSync('/missing.txt', 'x'), 100);
}
