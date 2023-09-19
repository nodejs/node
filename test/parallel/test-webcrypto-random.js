'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { Buffer } = require('buffer');
const assert = require('assert');
const { crypto } = globalThis;

[
  undefined, null, '', 1, {}, [],
  new Float32Array(1),
  new Float64Array(1),
  new DataView(new ArrayBuffer(1)),
].forEach((i) => {
  assert.throws(
    () => crypto.getRandomValues(i),
    { name: 'TypeMismatchError', code: 17 },
  );
});

{
  const buf = new Uint8Array(0);
  crypto.getRandomValues(buf);
}

const intTypedConstructors = [
  Int8Array,
  Int16Array,
  Int32Array,
  Uint8Array,
  Uint16Array,
  Uint32Array,
  Uint8ClampedArray,
  BigInt64Array,
  BigUint64Array,
];

for (const ctor of intTypedConstructors) {
  const buf = new ctor(10);
  const before = Buffer.from(buf.buffer).toString('hex');
  crypto.getRandomValues(buf);
  const after = Buffer.from(buf.buffer).toString('hex');
  assert.notStrictEqual(before, after);
}

{
  const buf = Buffer.alloc(10);
  const before = buf.toString('hex');
  crypto.getRandomValues(buf);
  const after = buf.toString('hex');
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
    assert.throws(
      () => crypto.getRandomValues(kData),
      { name: 'QuotaExceededError', code: 22 },
    );
  }
}

{
  const typedArray = new Uint8Array(32);
  assert.strictEqual(crypto.getRandomValues(typedArray), typedArray);
}
