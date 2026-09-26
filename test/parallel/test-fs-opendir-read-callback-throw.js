'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const dir = tmpdir.resolve('opendir-cb-throw');
fs.mkdirSync(dir);
fs.writeFileSync(`${dir}/a.txt`, '');
fs.writeFileSync(`${dir}/b.txt`, '');

// `dir.read(callback)` used to invoke `callback` from inside a `try` block, so
// an exception thrown by user code inside the callback was caught and fed back
// into the same callback as if it were a directory read failure. The callback
// must be invoked exactly once, and only with the result of the read.

process.on('uncaughtException', (err) => {
  assert.strictEqual(err.message, 'thrown from the read callback');
});

fs.opendir(dir, common.mustSucceed((d) => {
  d.read(common.mustCall((err, dirent) => {
    assert.strictEqual(err, null);
    assert.notStrictEqual(dirent, null);
    assert.match(dirent.name, /^[ab]\.txt$/);
    throw new Error('thrown from the read callback');
  }, 1));
}));
