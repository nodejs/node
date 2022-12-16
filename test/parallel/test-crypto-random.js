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

const kMaxInt32 = 2 ** 31 - 1;
const kMaxPossibleLength = Math.min(kMaxLength, kMaxInt32);

common.expectWarning('DeprecationWarning',
                     'crypto.pseudoRandomBytes is deprecated.', 'DEP0115');

{
  [crypto.randomBytes, crypto.pseudoRandomBytes].forEach((f) => {
    [undefined, null, false, true, {}, []].forEach((value) => {
      const errObj = {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message: 'The "size" argument must be of type number.' +
                 common.invalidArgTypeHelper(value)
      };
      assert.throws(() => f(value), errObj);
      assert.throws(() => f(value, common.mustNotCall()), errObj);
    });

    [-1, NaN, 2 ** 32, 2 ** 31].forEach((value) => {
      const errObj = {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
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
    new DataView(new ArrayBuffer(10)),
  ].forEach((buf) => {
    const before = Buffer.from(buf.buffer).toString('hex');
    crypto.randomFillSync(buf);
    const after = Buffer.from(buf.buffer).toString('hex');
    assert.notStrictEqual(before, after);
  });
}

{
  [
    new Uint16Array(10),
    new Uint32Array(10),
  ].forEach((buf) => {
    const before = Buffer.from(buf.buffer).toString('hex');
    globalThis.crypto.getRandomValues(buf);
    const after = Buffer.from(buf.buffer).toString('hex');
    assert.notStrictEqual(before, after);
  });
}

{
  [
    new ArrayBuffer(10),
    new SharedArrayBuffer(10),
  ].forEach((buf) => {
    const before = Buffer.from(buf).toString('hex');
    crypto.randomFillSync(buf);
    const after = Buffer.from(buf).toString('hex');
    assert.notStrictEqual(before, after);
  });
}

{
  const buf = Buffer.alloc(10);
  const before = buf.toString('hex');
  crypto.randomFill(buf, common.mustSucceed((buf) => {
    const after = buf.toString('hex');
    assert.notStrictEqual(before, after);
  }));
}

{
  const buf = new Uint8Array(new Array(10).fill(0));
  const before = Buffer.from(buf).toString('hex');
  crypto.randomFill(buf, common.mustSucceed((buf) => {
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
    new DataView(new ArrayBuffer(10)),
  ].forEach((buf) => {
    const before = Buffer.from(buf.buffer).toString('hex');
    crypto.randomFill(buf, common.mustSucceed((buf) => {
      const after = Buffer.from(buf.buffer).toString('hex');
      assert.notStrictEqual(before, after);
    }));
  });
}

{
  [
    new ArrayBuffer(10),
    new SharedArrayBuffer(10),
  ].forEach((buf) => {
    const before = Buffer.from(buf).toString('hex');
    crypto.randomFill(buf, common.mustSucceed((buf) => {
      const after = Buffer.from(buf).toString('hex');
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
  crypto.randomFill(buf, 5, 5, common.mustSucceed((buf) => {
    const after = buf.toString('hex');
    assert.notStrictEqual(before, after);
    assert.deepStrictEqual(before.slice(0, 5), after.slice(0, 5));
  }));
}

{
  const buf = new Uint8Array(new Array(10).fill(0));
  const before = Buffer.from(buf).toString('hex');
  crypto.randomFill(buf, 5, 5, common.mustSucceed((buf) => {
    const after = Buffer.from(buf).toString('hex');
    assert.notStrictEqual(before, after);
    assert.deepStrictEqual(before.slice(0, 5), after.slice(0, 5));
  }));
}

{
  [
    Buffer.alloc(10),
    new Uint8Array(new Array(10).fill(0)),
  ].forEach((buf) => {
    const len = Buffer.byteLength(buf);
    assert.strictEqual(len, 10, `Expected byteLength of 10, got ${len}`);

    const typeErrObj = {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "offset" argument must be of type number. ' +
               "Received type string ('test')"
    };

    assert.throws(() => crypto.randomFillSync(buf, 'test'), typeErrObj);

    assert.throws(
      () => crypto.randomFill(buf, 'test', common.mustNotCall()),
      typeErrObj);

    typeErrObj.message = typeErrObj.message.replace('offset', 'size');
    assert.throws(() => crypto.randomFillSync(buf, 0, 'test'), typeErrObj);

    assert.throws(
      () => crypto.randomFill(buf, 0, 'test', common.mustNotCall()),
      typeErrObj
    );

    [NaN, kMaxPossibleLength + 1, -10, (-1 >>> 0) + 1].forEach((offsetSize) => {
      const errObj = {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
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
      name: 'RangeError',
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
    name: 'RangeError',
    message: 'The value of "size" is out of range. ' +
             `It must be >= 0 && <= ${kMaxPossibleLength}. Received 4294967296`
  }
);

[1, true, NaN, null, undefined, {}, []].forEach((i) => {
  const buf = Buffer.alloc(10);
  assert.throws(
    () => crypto.randomFillSync(i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => crypto.randomFill(i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    }
  );
  assert.throws(
    () => crypto.randomFill(buf, 0, 10, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    });
});

[1, true, NaN, null, {}, []].forEach((i) => {
  assert.throws(
    () => crypto.randomBytes(1, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    }
  );
});

['pseudoRandomBytes', 'prng', 'rng'].forEach((f) => {
  const desc = Object.getOwnPropertyDescriptor(crypto, f);
  assert.ok(desc);
  assert.strictEqual(desc.configurable, true);
  assert.strictEqual(desc.enumerable, false);
});


{
  // Asynchronous API
  const randomInts = [];
  for (let i = 0; i < 100; i++) {
    crypto.randomInt(3, common.mustSucceed((n) => {
      assert.ok(n >= 0);
      assert.ok(n < 3);
      randomInts.push(n);
      if (randomInts.length === 100) {
        assert.ok(!randomInts.includes(-1));
        assert.ok(randomInts.includes(0));
        assert.ok(randomInts.includes(1));
        assert.ok(randomInts.includes(2));
        assert.ok(!randomInts.includes(3));
      }
    }));
  }
}
{
  // Synchronous API
  const randomInts = [];
  for (let i = 0; i < 100; i++) {
    const n = crypto.randomInt(3);
    assert.ok(n >= 0);
    assert.ok(n < 3);
    randomInts.push(n);
  }

  assert.ok(!randomInts.includes(-1));
  assert.ok(randomInts.includes(0));
  assert.ok(randomInts.includes(1));
  assert.ok(randomInts.includes(2));
  assert.ok(!randomInts.includes(3));
}
{
  // Positive range
  const randomInts = [];
  for (let i = 0; i < 100; i++) {
    crypto.randomInt(1, 3, common.mustSucceed((n) => {
      assert.ok(n >= 1);
      assert.ok(n < 3);
      randomInts.push(n);
      if (randomInts.length === 100) {
        assert.ok(!randomInts.includes(0));
        assert.ok(randomInts.includes(1));
        assert.ok(randomInts.includes(2));
        assert.ok(!randomInts.includes(3));
      }
    }));
  }
}
{
  // Negative range
  const randomInts = [];
  for (let i = 0; i < 100; i++) {
    crypto.randomInt(-10, -8, common.mustSucceed((n) => {
      assert.ok(n >= -10);
      assert.ok(n < -8);
      randomInts.push(n);
      if (randomInts.length === 100) {
        assert.ok(!randomInts.includes(-11));
        assert.ok(randomInts.includes(-10));
        assert.ok(randomInts.includes(-9));
        assert.ok(!randomInts.includes(-8));
      }
    }));
  }
}
{

  ['10', true, NaN, null, {}, []].forEach((i) => {
    const invalidMinError = {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "min" argument must be a safe integer.' +
               `${common.invalidArgTypeHelper(i)}`,
    };
    const invalidMaxError = {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "max" argument must be a safe integer.' +
               `${common.invalidArgTypeHelper(i)}`,
    };

    assert.throws(
      () => crypto.randomInt(i, 100),
      invalidMinError
    );
    assert.throws(
      () => crypto.randomInt(i, 100, common.mustNotCall()),
      invalidMinError
    );
    assert.throws(
      () => crypto.randomInt(i),
      invalidMaxError
    );
    assert.throws(
      () => crypto.randomInt(i, common.mustNotCall()),
      invalidMaxError
    );
    assert.throws(
      () => crypto.randomInt(0, i, common.mustNotCall()),
      invalidMaxError
    );
    assert.throws(
      () => crypto.randomInt(0, i),
      invalidMaxError
    );
  });

  const maxInt = Number.MAX_SAFE_INTEGER;
  const minInt = Number.MIN_SAFE_INTEGER;

  crypto.randomInt(minInt, minInt + 5, common.mustSucceed());
  crypto.randomInt(maxInt - 5, maxInt, common.mustSucceed());

  assert.throws(
    () => crypto.randomInt(minInt - 1, minInt + 5, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "min" argument must be a safe integer.' +
      `${common.invalidArgTypeHelper(minInt - 1)}`,
    }
  );

  assert.throws(
    () => crypto.randomInt(maxInt + 1, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "max" argument must be a safe integer.' +
      `${common.invalidArgTypeHelper(maxInt + 1)}`,
    }
  );

  crypto.randomInt(1, common.mustSucceed());
  crypto.randomInt(0, 1, common.mustSucceed());
  for (const arg of [[0], [1, 1], [3, 2], [-5, -5], [11, -10]]) {
    assert.throws(() => crypto.randomInt(...arg, common.mustNotCall()), {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
      message: 'The value of "max" is out of range. It must be greater than ' +
      `the value of "min" (${arg[arg.length - 2] || 0}). ` +
      `Received ${arg[arg.length - 1]}`
    });
  }

  const MAX_RANGE = 0xFFFF_FFFF_FFFF;
  crypto.randomInt(MAX_RANGE, common.mustSucceed());
  crypto.randomInt(1, MAX_RANGE + 1, common.mustSucceed());
  assert.throws(
    () => crypto.randomInt(1, MAX_RANGE + 2, common.mustNotCall()),
    {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
      message: 'The value of "max - min" is out of range. ' +
               `It must be <= ${MAX_RANGE}. ` +
               'Received 281_474_976_710_656'
    }
  );

  assert.throws(() => crypto.randomInt(MAX_RANGE + 1, common.mustNotCall()), {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: 'The value of "max" is out of range. ' +
             `It must be <= ${MAX_RANGE}. ` +
             'Received 281_474_976_710_656'
  });

  [true, NaN, null, {}, [], 10].forEach((i) => {
    const cbError = {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
    };
    assert.throws(() => crypto.randomInt(0, 1, i), cbError);
  });
}

{
  // Verify that it doesn't throw or abort
  crypto.randomFill(new Uint16Array(10), 0, common.mustSucceed());
  crypto.randomFill(new Uint32Array(10), 0, common.mustSucceed());
  crypto.randomFill(new Uint32Array(10), 0, 1, common.mustSucceed());
}
