// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// Flags: --pending-deprecation
'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const { kMaxLength } = require('buffer');

const kMaxUint32 = Math.pow(2, 32) - 1;
const kMaxPossibleLength = Math.min(kMaxLength, kMaxUint32);

// bump, we register a lot of exit listeners
process.setMaxListeners(256);

common.expectWarning('DeprecationWarning',
                     'crypto.pseudoRandomBytes is deprecated.', 'DEP0115');

{
  [crypto.randomBytes, crypto.pseudoRandomBytes].forEach((f) => {
    [undefined, null, false, true, {}, []].forEach((value) => {
      const errObj = {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError [ERR_INVALID_ARG_TYPE]',
        message: 'The "size" argument must be of type number. ' +
                `Received type ${typeof value}`
      };
      assert.throws(() => f(value), errObj);
      assert.throws(() => f(value, common.mustNotCall()), errObj);
    });

    [-1, NaN, 2 ** 32].forEach((value) => {
      const errObj = {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError [ERR_OUT_OF_RANGE]',
        message: 'The value of "size" is out of range. It must be >= 0 && <= ' +
                 `${kMaxPossibleLength}. Received ${value}`
      };
      assert.throws(() => f(value), errObj);
      assert.throws(() => f(value, common.mustNotCall()), errObj);
    });

    [0, 1, 2, 4, 16, 256, 1024, 101.2].forEach((len) => {
      f(len, common.mustCall((ex, buf) => {
        assert.strictEqual(ex, null);
        assert.strictEqual(buf.length, Math.floor(len));
        assert.ok(Buffer.isBuffer(buf));
      }));
    });
  });
}

{
  const buf = Buffer.alloc(10);
  const before = buf.toString('hex');
  const after = crypto.randomFillSync(buf).toString('hex');
  assert.notStrictEqual(before, after);
}

{
  const buf = new Uint8Array(new Array(10).fill(0));
  const before = Buffer.from(buf).toString('hex');
  crypto.randomFillSync(buf);
  const after = Buffer.from(buf).toString('hex');
  assert.notStrictEqual(before, after);
}

{
  [
    new Uint16Array(10),
    new Uint32Array(10),
    new Float32Array(10),
    new Float64Array(10),
    new DataView(new ArrayBuffer(10))
  ].forEach((buf) => {
    const before = Buffer.from(buf.buffer).toString('hex');
    crypto.randomFillSync(buf);
    const after = Buffer.from(buf.buffer).toString('hex');
    assert.notStrictEqual(before, after);
  });
}

{
  const buf = Buffer.alloc(10);
  const before = buf.toString('hex');
  crypto.randomFill(buf, common.mustCall((err, buf) => {
    assert.ifError(err);
    const after = buf.toString('hex');
    assert.notStrictEqual(before, after);
  }));
}

{
  const buf = new Uint8Array(new Array(10).fill(0));
  const before = Buffer.from(buf).toString('hex');
  crypto.randomFill(buf, common.mustCall((err, buf) => {
    assert.ifError(err);
    const after = Buffer.from(buf).toString('hex');
    assert.notStrictEqual(before, after);
  }));
}

{
  [
    new Uint16Array(10),
    new Uint32Array(10),
    new Float32Array(10),
    new Float64Array(10),
    new DataView(new ArrayBuffer(10))
  ].forEach((buf) => {
    const before = Buffer.from(buf.buffer).toString('hex');
    crypto.randomFill(buf, common.mustCall((err, buf) => {
      assert.ifError(err);
      const after = Buffer.from(buf.buffer).toString('hex');
      assert.notStrictEqual(before, after);
    }));
  });
}

{
  const buf = Buffer.alloc(10);
  const before = buf.toString('hex');
  crypto.randomFillSync(buf, 5, 5);
  const after = buf.toString('hex');
  assert.notStrictEqual(before, after);
  assert.deepStrictEqual(before.slice(0, 5), after.slice(0, 5));
}

{
  const buf = new Uint8Array(new Array(10).fill(0));
  const before = Buffer.from(buf).toString('hex');
  crypto.randomFillSync(buf, 5, 5);
  const after = Buffer.from(buf).toString('hex');
  assert.notStrictEqual(before, after);
  assert.deepStrictEqual(before.slice(0, 5), after.slice(0, 5));
}

{
  const buf = Buffer.alloc(10);
  const before = buf.toString('hex');
  crypto.randomFillSync(buf, 5);
  const after = buf.toString('hex');
  assert.notStrictEqual(before, after);
  assert.deepStrictEqual(before.slice(0, 5), after.slice(0, 5));
}

{
  const buf = Buffer.alloc(10);
  const before = buf.toString('hex');
  crypto.randomFill(buf, 5, 5, common.mustCall((err, buf) => {
    assert.ifError(err);
    const after = buf.toString('hex');
    assert.notStrictEqual(before, after);
    assert.deepStrictEqual(before.slice(0, 5), after.slice(0, 5));
  }));
}

{
  const buf = new Uint8Array(new Array(10).fill(0));
  const before = Buffer.from(buf).toString('hex');
  crypto.randomFill(buf, 5, 5, common.mustCall((err, buf) => {
    assert.ifError(err);
    const after = Buffer.from(buf).toString('hex');
    assert.notStrictEqual(before, after);
    assert.deepStrictEqual(before.slice(0, 5), after.slice(0, 5));
  }));
}

{
  [
    Buffer.alloc(10),
    new Uint8Array(new Array(10).fill(0))
  ].forEach((buf) => {
    const len = Buffer.byteLength(buf);
    assert.strictEqual(len, 10, `Expected byteLength of 10, got ${len}`);

    const typeErrObj = {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError [ERR_INVALID_ARG_TYPE]',
      message: 'The "offset" argument must be of type number. ' +
               'Received type string'
    };

    assert.throws(() => crypto.randomFillSync(buf, 'test'), typeErrObj);

    assert.throws(
      () => crypto.randomFill(buf, 'test', common.mustNotCall()),
      typeErrObj);

    typeErrObj.message = 'The "size" argument must be of type number. ' +
                     'Received type string';
    assert.throws(() => crypto.randomFillSync(buf, 0, 'test'), typeErrObj);

    assert.throws(
      () => crypto.randomFill(buf, 0, 'test', common.mustNotCall()),
      typeErrObj
    );

    [NaN, kMaxPossibleLength + 1, -10, (-1 >>> 0) + 1].forEach((offsetSize) => {
      const errObj = {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError [ERR_OUT_OF_RANGE]',
        message: 'The value of "offset" is out of range. ' +
                 `It must be >= 0 && <= 10. Received ${offsetSize}`
      };

      assert.throws(() => crypto.randomFillSync(buf, offsetSize), errObj);

      assert.throws(
        () => crypto.randomFill(buf, offsetSize, common.mustNotCall()),
        errObj);

      errObj.message = 'The value of "size" is out of range. It must be >= ' +
                       `0 && <= ${kMaxPossibleLength}. Received ${offsetSize}`;
      assert.throws(() => crypto.randomFillSync(buf, 1, offsetSize), errObj);

      assert.throws(
        () => crypto.randomFill(buf, 1, offsetSize, common.mustNotCall()),
        errObj
      );
    });

    const rangeErrObj = {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError [ERR_OUT_OF_RANGE]',
      message: 'The value of "size + offset" is out of range. ' +
               'It must be <= 10. Received 11'
    };
    assert.throws(() => crypto.randomFillSync(buf, 1, 10), rangeErrObj);

    assert.throws(
      () => crypto.randomFill(buf, 1, 10, common.mustNotCall()),
      rangeErrObj
    );
  });
}

// https://github.com/nodejs/node-v0.x-archive/issues/5126,
// "FATAL ERROR: v8::Object::SetIndexedPropertiesToExternalArrayData() length
// exceeds max acceptable value"
assert.throws(
  () => crypto.randomBytes((-1 >>> 0) + 1),
  {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError [ERR_OUT_OF_RANGE]',
    message: 'The value of "size" is out of range. ' +
             `It must be >= 0 && <= ${kMaxPossibleLength}. Received 4294967296`
  }
);

[1, true, NaN, null, undefined, {}, []].forEach((i) => {
  const buf = Buffer.alloc(10);
  common.expectsError(
    () => crypto.randomFillSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => crypto.randomFill(i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => crypto.randomFill(buf, 0, 10, i),
    {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError,
      message: 'Callback must be a function',
    });
});

[1, true, NaN, null, {}, []].forEach((i) => {
  common.expectsError(
    () => crypto.randomBytes(1, i),
    {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError,
      message: 'Callback must be a function',
    }
  );
});


['pseudoRandomBytes', 'prng', 'rng'].forEach((f) => {
  const desc = Object.getOwnPropertyDescriptor(crypto, f);
  assert.ok(desc);
  assert.strictEqual(desc.configurable, true);
  assert.strictEqual(desc.writable, true);
  assert.strictEqual(desc.enumerable, false);
});
