'use strict';

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

const {
  Z_PARTIAL_FLUSH, Z_SYNC_FLUSH, Z_FULL_FLUSH, Z_FINISH
} = zlib.constants;

common.crashOnUnhandledRejection();

async function getOutput(...sequenceOfFlushes) {
  const zipper = zlib.createGzip({ highWaterMark: 16384 });

  zipper.write('A'.repeat(17000));
  for (const flush of sequenceOfFlushes) {
    zipper.flush(flush);
  }

  const data = [];

  return new Promise((resolve) => {
    zipper.on('data', common.mustCall((d) => {
      data.push(d);
      if (data.length === 2) resolve(Buffer.concat(data));
    }, 2));
  });
}

(async function() {
  assert.deepStrictEqual(await getOutput(Z_SYNC_FLUSH),
                         await getOutput(Z_SYNC_FLUSH, Z_PARTIAL_FLUSH));
  assert.deepStrictEqual(await getOutput(Z_SYNC_FLUSH),
                         await getOutput(Z_PARTIAL_FLUSH, Z_SYNC_FLUSH));

  assert.deepStrictEqual(await getOutput(Z_FINISH),
                         await getOutput(Z_FULL_FLUSH, Z_FINISH));
  assert.deepStrictEqual(await getOutput(Z_FINISH),
                         await getOutput(Z_SYNC_FLUSH, Z_FINISH));
})();
