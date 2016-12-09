'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const assert = require('assert');
const crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

// bump, we register a lot of exit listeners
process.setMaxListeners(256);

const expectedErrorRegexp = /^TypeError: size must be a number >= 0$/;
[crypto.randomBytes, crypto.pseudoRandomBytes].forEach(function(f) {
  [-1, undefined, null, false, true, {}, []].forEach(function(value) {
    assert.throws(function() { f(value); }, expectedErrorRegexp);
    assert.throws(function() { f(value, function() {}); }, expectedErrorRegexp);
  });

  [0, 1, 2, 4, 16, 256, 1024].forEach(function(len) {
    f(len, common.mustCall(function(ex, buf) {
      assert.strictEqual(ex, null);
      assert.strictEqual(buf.length, len);
      assert.ok(Buffer.isBuffer(buf));
    }));
  });
});

const noop = () => {};

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

  for (const buf of bufs) {
    const len = Buffer.byteLength(buf);
    assert.strictEqual(len, 10, `Expected byteLength of 10, got ${len}`);

    assert.throws(() => {
      crypto.randomFillSync(buf, 'test');
    }, /offset must be a number/);

    assert.throws(() => {
      crypto.randomFillSync(buf, NaN);
    }, /offset must be a number/);

    assert.throws(() => {
      crypto.randomFill(buf, 'test', noop);
    }, /offset must be a number/);

    assert.throws(() => {
      crypto.randomFill(buf, NaN, noop);
    }, /offset must be a number/);

    const max = require('buffer').kMaxLength + 1;

    assert.throws(() => {
      crypto.randomFillSync(buf, 11);
    }, /offset out of range/);

    assert.throws(() => {
      crypto.randomFillSync(buf, max);
    }, /offset out of range/);

    assert.throws(() => {
      crypto.randomFill(buf, 11, noop);
    }, /offset out of range/);

    assert.throws(() => {
      crypto.randomFill(buf, max, noop);
    }, /offset out of range/);

    assert.throws(() => {
      crypto.randomFillSync(buf, 0, 'test');
    }, /size must be a number/);

    assert.throws(() => {
      crypto.randomFillSync(buf, 0, NaN);
    }, /size must be a number/);

    assert.throws(() => {
      crypto.randomFill(buf, 0, 'test', noop);
    }, /size must be a number/);

    assert.throws(() => {
      crypto.randomFill(buf, 0, NaN, noop);
    }, /size must be a number/);

    {
      const size = (-1 >>> 0) + 1;

      assert.throws(() => {
        crypto.randomFillSync(buf, 0, -10);
      }, /size must be a uint32/);

      assert.throws(() => {
        crypto.randomFillSync(buf, 0, size);
      }, /size must be a uint32/);

      assert.throws(() => {
        crypto.randomFill(buf, 0, -10, noop);
      }, /size must be a uint32/);

      assert.throws(() => {
        crypto.randomFill(buf, 0, size, noop);
      }, /size must be a uint32/);
    }

    assert.throws(() => {
      crypto.randomFillSync(buf, -10);
    }, /offset must be a uint32/);

    assert.throws(() => {
      crypto.randomFill(buf, -10, noop);
    }, /offset must be a uint32/);

    assert.throws(() => {
      crypto.randomFillSync(buf, 1, 10);
    }, /buffer too small/);

    assert.throws(() => {
      crypto.randomFill(buf, 1, 10, noop);
    }, /buffer too small/);

    assert.throws(() => {
      crypto.randomFillSync(buf, 0, 12);
    }, /buffer too small/);

    assert.throws(() => {
      crypto.randomFill(buf, 0, 12, noop);
    }, /buffer too small/);

    {
      // Offset is too big
      const offset = (-1 >>> 0) + 1;
      assert.throws(() => {
        crypto.randomFillSync(buf, offset, 10);
      }, /offset must be a uint32/);

      assert.throws(() => {
        crypto.randomFill(buf, offset, 10, noop);
      }, /offset must be a uint32/);
    }
  }
}

// #5126, "FATAL ERROR: v8::Object::SetIndexedPropertiesToExternalArrayData()
// length exceeds max acceptable value"
assert.throws(function() {
  crypto.randomBytes((-1 >>> 0) + 1);
}, TypeError);
