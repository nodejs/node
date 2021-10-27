'use strict';
const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

// Tests that zlib streams support .reset() and .params()
// before the first write. That is important to ensure that
// lazy init of zlib native library handles these cases.

for (const fn of [
  (z, cb) => {
    z.reset();
    cb();
  },
  (z, cb) => z.params(0, zlib.constants.Z_DEFAULT_STRATEGY, cb),
]) {
  const deflate = zlib.createDeflate();
  const inflate = zlib.createInflate();

  deflate.pipe(inflate);

  const output = [];
  inflate
    .on('error', (err) => {
      assert.ifError(err);
    })
    .on('data', (chunk) => output.push(chunk))
    .on('end', common.mustCall(
      () => assert.strictEqual(Buffer.concat(output).toString(), 'abc')));

  fn(deflate, () => {
    fn(inflate, () => {
      deflate.write('abc');
      deflate.end();
    });
  });
}
