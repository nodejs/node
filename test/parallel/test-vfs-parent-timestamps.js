// Flags: --experimental-vfs
'use strict';

// Operations that modify a directory should bump its mtime/ctime.

const common = require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/dir');

function getTimestamps(p) {
  const st = myVfs.statSync(p);
  return { mtimeMs: st.mtimeMs, ctimeMs: st.ctimeMs };
}

const before = getTimestamps('/dir');
// Wait long enough for ms-resolution mtime to differ
setTimeout(common.mustCall(() => {
  myVfs.writeFileSync('/dir/file.txt', 'hello');
  const after = getTimestamps('/dir');
  assert.ok(after.mtimeMs >= before.mtimeMs);
  assert.ok(after.ctimeMs >= before.ctimeMs);
}), 5);
