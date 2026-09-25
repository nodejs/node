// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { from, pull, bytes } = require('stream/iter');
const { compressBrotli, compressZstd } = require('zlib/iter');

// Type validation of options.params in zlib/iter transforms: plain
// objects and arrays pass the check, any other value rejects with
// ERR_INVALID_ARG_TYPE. Arrays have always passed the typeof-based
// check, so this behavior must be preserved by any refactor.

const consume = (transform) => bytes(pull(from('test'), transform));

(async () => {
  for (const compress of [compressBrotli, compressZstd]) {
    for (const params of [42, 'bad', true, Symbol(), () => {}, null]) {
      await assert.rejects(
        consume(compress({ params })),
        { code: 'ERR_INVALID_ARG_TYPE' },
      );
    }

    // An empty array has no own keys, so it passes both the type check
    // and the per-key validation and compression succeeds.
    const out = await consume(compress({ params: [] }));
    assert.ok(out.byteLength > 0);
  }
})().then(common.mustCall());
