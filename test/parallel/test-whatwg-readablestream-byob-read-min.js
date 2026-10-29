'use strict';
const common = require('../common');
const assert = require('node:assert');

const {
  ReadableStream,
} = require('node:stream/web');

// Validation of the options.min argument of ReadableStreamBYOBReader.read()
// must reject with the same errors regardless of how the checks are implemented internally.

const reader = new ReadableStream({ type: 'bytes' })
  .getReader({ mode: 'byob' });

(async () => {
  // A null min is not covered here: `options?.min ?? 1` turns it into
  // the default before validation, so it never reaches the type check.
  for (const min of ['1', true, {}, [], 1n]) {
    await assert.rejects(
      reader.read(new Uint8Array(8), { min }),
      { code: 'ERR_INVALID_ARG_TYPE' },
    );
  }

  for (const min of [NaN, 1.5, 0, -1]) {
    await assert.rejects(
      reader.read(new Uint8Array(8), { min }),
      { code: 'ERR_INVALID_ARG_VALUE' },
    );
  }

  await assert.rejects(
    reader.read(new Uint8Array(8), { min: 9 }),
    { code: 'ERR_OUT_OF_RANGE' },
  );

  await assert.rejects(
    reader.read(new DataView(new ArrayBuffer(8)), { min: 9 }),
    { code: 'ERR_OUT_OF_RANGE' },
  );
})().then(common.mustCall());
