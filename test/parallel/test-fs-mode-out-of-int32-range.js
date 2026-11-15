'use strict';

// This tests that a mode above INT32_MAX is reported to the caller as a
// RangeError instead of failing a CHECK in the native binding and aborting
// the process.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const file = tmpdir.resolve('file.txt');
fs.writeFileSync(file, 'x');

for (const mode of [2 ** 31, 2176057344, 2 ** 32 - 1]) {
  const expected = (name) => ({
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: `The value of "${name}" is out of range. It must be >= 0 && <= ` +
             `2147483647. Received ${mode}`,
  });
  assert.throws(() => fs.openSync(file, 'w', mode), expected('mode'));
  assert.throws(() => fs.open(file, 'w', mode, common.mustNotCall()),
                expected('mode'));
  assert.rejects(fs.promises.open(file, 'w', mode), expected('mode'))
    .then(common.mustCall());
  assert.throws(() => fs.chmodSync(file, mode), expected('mode'));
  assert.throws(() => fs.mkdirSync(tmpdir.resolve('dir'), { mode }),
                expected('options.mode'));
  assert.throws(
    () => fs.mkdirSync(tmpdir.resolve('a/b'), { recursive: true, mode }),
    expected('options.mode'),
  );
  assert.throws(() => fs.createReadStream(file, { mode }),
                expected('options.mode'));
  assert.throws(() => fs.createWriteStream(file, { mode }),
                expected('options.mode'));
}

// The largest signed 32-bit value is still accepted.
fs.closeSync(fs.openSync(file, 'w', 2 ** 31 - 1));
fs.createWriteStream(file, { mode: 0o644 })
  .on('open', common.mustCall(function() { this.close(); }));
