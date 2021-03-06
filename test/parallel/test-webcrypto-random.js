'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { Buffer } = require('buffer');
const assert = require('assert');
const { getRandomValues } = require('crypto').webcrypto;

[undefined, null, '', 1, {}, []].forEach((i) => {
  assert.throws(() => getRandomValues(i), { code: 17 });
});

{
  const buf = new Uint8Array(0);
  getRandomValues(buf);
}

{
  const buf = new Uint8Array(new Array(10).fill(0));
  const before = Buffer.from(buf).toString('hex');
  getRandomValues(buf);
  const after = Buffer.from(buf).toString('hex');
  assert.notStrictEqual(before, after);
}

{
  const buf = new Uint16Array(new Array(10).fill(0));
  const before = Buffer.from(buf).toString('hex');
  getRandomValues(buf);
  const after = Buffer.from(buf).toString('hex');
  assert.notStrictEqual(before, after);
}

{
  const buf = new Uint32Array(new Array(10).fill(0));
  const before = Buffer.from(buf).toString('hex');
  getRandomValues(buf);
  const after = Buffer.from(buf).toString('hex');
  assert.notStrictEqual(before, after);
}

{
  const buf = new Uint16Array(new Array(10).fill(0));
  const before = Buffer.from(buf).toString('hex');
  getRandomValues(new DataView(buf.buffer));
  const after = Buffer.from(buf).toString('hex');
  assert.notStrictEqual(before, after);
}

{
  let kData;
  try {
    kData = Buffer.alloc(65536 + 1);
  } catch {
    // Ignore if error here.
  }

  if (kData !== undefined) {
    assert.throws(() => getRandomValues(kData), {
      code: 22
    });
  }
}
