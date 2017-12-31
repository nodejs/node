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

'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

// bump, we register a lot of exit listeners
process.setMaxListeners(256);

[crypto.randomBytes, crypto.pseudoRandomBytes].forEach(function(f) {
  [-1, undefined, null, false, true, {}, []].forEach(function(value) {

    common.expectsError(
      () => f(value),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: /^The "size" argument must be of type (number|uint32)$/
      }
    );

    common.expectsError(
      () => f(value, common.mustNotCall()),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: /^The "size" argument must be of type (number|uint32)$/
      }
    );
  });

  [0, 1, 2, 4, 16, 256, 1024, 101.2].forEach(function(len) {
    f(len, common.mustCall(function(ex, buf) {
      assert.strictEqual(ex, null);
      assert.strictEqual(buf.length, Math.floor(len));
      assert.ok(Buffer.isBuffer(buf));
    }));
  });
});

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
  const buf = new Uint16Array(10);
  const before = Buffer.from(buf.buffer).toString('hex');
  crypto.randomFillSync(buf);
  const after = Buffer.from(buf.buffer).toString('hex');
  assert.notStrictEqual(before, after);
}

{
  const buf = new Uint32Array(10);
  const before = Buffer.from(buf.buffer).toString('hex');
  crypto.randomFillSync(buf);
  const after = Buffer.from(buf.buffer).toString('hex');
  assert.notStrictEqual(before, after);
}

{
  const buf = new Float32Array(10);
  const before = Buffer.from(buf.buffer).toString('hex');
  crypto.randomFillSync(buf);
  const after = Buffer.from(buf.buffer).toString('hex');
  assert.notStrictEqual(before, after);
}

{
  const buf = new Float64Array(10);
  const before = Buffer.from(buf.buffer).toString('hex');
  crypto.randomFillSync(buf);
  const after = Buffer.from(buf.buffer).toString('hex');
  assert.notStrictEqual(before, after);
}

{
  const buf = new DataView(new ArrayBuffer(10));
  const before = Buffer.from(buf.buffer).toString('hex');
  crypto.randomFillSync(buf);
  const after = Buffer.from(buf.buffer).toString('hex');
  assert.notStrictEqual(before, after);
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
  const buf = new Uint16Array(10);
  const before = Buffer.from(buf.buffer).toString('hex');
  crypto.randomFill(buf, common.mustCall((err, buf) => {
    assert.ifError(err);
    const after = Buffer.from(buf.buffer).toString('hex');
    assert.notStrictEqual(before, after);
  }));
}

{
  const buf = new Uint32Array(10);
  const before = Buffer.from(buf.buffer).toString('hex');
  crypto.randomFill(buf, common.mustCall((err, buf) => {
    assert.ifError(err);
    const after = Buffer.from(buf.buffer).toString('hex');
    assert.notStrictEqual(before, after);
  }));
}

{
  const buf = new Float32Array(10);
  const before = Buffer.from(buf.buffer).toString('hex');
  crypto.randomFill(buf, common.mustCall((err, buf) => {
    assert.ifError(err);
    const after = Buffer.from(buf.buffer).toString('hex');
    assert.notStrictEqual(before, after);
  }));
}

{
  const buf = new Float64Array(10);
  const before = Buffer.from(buf.buffer).toString('hex');
  crypto.randomFill(buf, common.mustCall((err, buf) => {
    assert.ifError(err);
    const after = Buffer.from(buf.buffer).toString('hex');
    assert.notStrictEqual(before, after);
  }));
}

{
  const buf = new DataView(new ArrayBuffer(10));
  const before = Buffer.from(buf.buffer).toString('hex');
  crypto.randomFill(buf, common.mustCall((err, buf) => {
    assert.ifError(err);
    const after = Buffer.from(buf.buffer).toString('hex');
    assert.notStrictEqual(before, after);
  }));
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
  const bufs = [
    Buffer.alloc(10),
    new Uint8Array(new Array(10).fill(0))
  ];

  const max = require('buffer').kMaxLength + 1;

  for (const buf of bufs) {
    const len = Buffer.byteLength(buf);
    assert.strictEqual(len, 10, `Expected byteLength of 10, got ${len}`);

    common.expectsError(
      () => crypto.randomFillSync(buf, 'test'),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "offset" argument must be of type number'
      }
    );

    common.expectsError(
      () => crypto.randomFillSync(buf, NaN),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "offset" argument must be of type number'
      }
    );

    common.expectsError(
      () => crypto.randomFill(buf, 'test', common.mustNotCall()),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "offset" argument must be of type number'
      }
    );

    common.expectsError(
      () => crypto.randomFill(buf, NaN, common.mustNotCall()),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "offset" argument must be of type number'
      }
    );

    common.expectsError(
      () => crypto.randomFillSync(buf, 11),
      {
        code: 'ERR_OUT_OF_RANGE',
        type: RangeError,
        message: 'The value of "offset" is out of range.'
      }
    );

    common.expectsError(
      () => crypto.randomFillSync(buf, max),
      {
        code: 'ERR_OUT_OF_RANGE',
        type: RangeError,
        message: 'The value of "offset" is out of range.'
      }
    );

    common.expectsError(
      () => crypto.randomFill(buf, 11, common.mustNotCall()),
      {
        code: 'ERR_OUT_OF_RANGE',
        type: RangeError,
        message: 'The value of "offset" is out of range.'
      }
    );

    common.expectsError(
      () => crypto.randomFill(buf, max, common.mustNotCall()),
      {
        code: 'ERR_OUT_OF_RANGE',
        type: RangeError,
        message: 'The value of "offset" is out of range.'
      }
    );

    common.expectsError(
      () => crypto.randomFillSync(buf, 0, 'test'),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "size" argument must be of type number'
      }
    );

    common.expectsError(
      () => crypto.randomFillSync(buf, 0, NaN),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "size" argument must be of type number'
      }
    );

    common.expectsError(
      () => crypto.randomFill(buf, 0, 'test', common.mustNotCall()),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "size" argument must be of type number'
      }
    );

    common.expectsError(
      () => crypto.randomFill(buf, 0, NaN, common.mustNotCall()),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "size" argument must be of type number'
      }
    );

    {
      const size = (-1 >>> 0) + 1;

      common.expectsError(
        () => crypto.randomFillSync(buf, 0, -10),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          type: TypeError,
          message: 'The "size" argument must be of type uint32'
        }
      );

      common.expectsError(
        () => crypto.randomFillSync(buf, 0, size),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          type: TypeError,
          message: 'The "size" argument must be of type uint32'
        }
      );

      common.expectsError(
        () => crypto.randomFill(buf, 0, -10, common.mustNotCall()),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          type: TypeError,
          message: 'The "size" argument must be of type uint32'
        }
      );

      common.expectsError(
        () => crypto.randomFill(buf, 0, size, common.mustNotCall()),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          type: TypeError,
          message: 'The "size" argument must be of type uint32'
        }
      );
    }

    common.expectsError(
      () => crypto.randomFillSync(buf, -10),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "offset" argument must be of type uint32'
      }
    );

    common.expectsError(
      () => crypto.randomFill(buf, -10, common.mustNotCall()),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "offset" argument must be of type uint32'
      }
    );

    common.expectsError(
      () => crypto.randomFillSync(buf, 1, 10),
      {
        code: 'ERR_OUT_OF_RANGE',
        type: RangeError,
        message: 'The value of "size" is out of range.'
      }
    );

    common.expectsError(
      () => crypto.randomFill(buf, 1, 10, common.mustNotCall()),
      {
        code: 'ERR_OUT_OF_RANGE',
        type: RangeError,
        message: 'The value of "size" is out of range.'
      }
    );

    common.expectsError(
      () => crypto.randomFillSync(buf, 0, 12),
      {
        code: 'ERR_OUT_OF_RANGE',
        type: RangeError,
        message: 'The value of "size" is out of range.'
      }
    );

    common.expectsError(
      () => crypto.randomFill(buf, 0, 12, common.mustNotCall()),
      {
        code: 'ERR_OUT_OF_RANGE',
        type: RangeError,
        message: 'The value of "size" is out of range.'
      }
    );

    {
      // Offset is too big
      const offset = (-1 >>> 0) + 1;
      common.expectsError(
        () => crypto.randomFillSync(buf, offset, 10),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          type: TypeError,
          message: 'The "offset" argument must be of type uint32'
        }
      );

      common.expectsError(
        () => crypto.randomFill(buf, offset, 10, common.mustNotCall()),
        {
          code: 'ERR_INVALID_ARG_TYPE',
          type: TypeError,
          message: 'The "offset" argument must be of type uint32'
        }
      );
    }
  }
}

// https://github.com/nodejs/node-v0.x-archive/issues/5126,
// "FATAL ERROR: v8::Object::SetIndexedPropertiesToExternalArrayData() length
// exceeds max acceptable value"
common.expectsError(
  () => crypto.randomBytes((-1 >>> 0) + 1),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "size" argument must be of type uint32'
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
