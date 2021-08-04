'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { Buffer } = require('buffer');
const assert = require('assert');
const { getRandomValues } = require('crypto').webcrypto;

[
  undefined, null, '', 1, {}, [],
  new Float32Array(1),
  new Float64Array(1),
].forEach((i) => {
  assert.throws(
    () => getRandomValues(i),
    { name: 'TypeMismatchError', code: 17 },
  );
});

{
  const buf = new Uint8Array(0);
  getRandomValues(buf);
}

const intTypedConstructors = [
  Int8Array,
  Int16Array,
  Int32Array,
  Uint8Array,
  Uint16Array,
  Uint32Array,
  BigInt64Array,
  BigUint64Array,
];

for (const ctor of intTypedConstructors) {
  const buf = new ctor(10);
  const before = Buffer.from(buf.buffer).toString('hex');
  getRandomValues(buf);
  const after = Buffer.from(buf.buffer).toString('hex');
  assert.notStrictEqual(before, after);
}

{
  const buf = new Uint16Array(10);
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
