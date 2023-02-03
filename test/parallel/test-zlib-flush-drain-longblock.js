'use strict';

// Regression test for https://github.com/nodejs/node/issues/14523.
// Checks that flushes interact properly with writableState.needDrain,
// even if no flush callback was passed.

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const zipper = zlib.createGzip({ highWaterMark: 16384 });
const unzipper = zlib.createGunzip();
zipper.pipe(unzipper);

zipper.write('A'.repeat(17000));
zipper.flush();

let received = 0;
unzipper.on('data', common.mustCallAtLeast((d) => {
  received += d.length;
}, 2));

// Properly `.end()`ing the streams would interfere with checking that
// `.flush()` works.
process.on('exit', () => {
  assert.strictEqual(received, 17000);
});
