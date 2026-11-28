// Flags: --js-immutable-arraybuffer --expose-internals --allow-natives-syntax --no-warnings
'use strict';

const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { utf8Write } = require('internal/buffer');

if (typeof ArrayBuffer.prototype.transferToImmutable !== 'function')
  common.skip('ArrayBuffer.prototype.transferToImmutable is not available');

const immutableError = {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
  message: "The argument 'buffer' must not be backed by an immutable ArrayBuffer",
};

function immutable(bytes) {
  return Buffer.from(Uint8Array.from(bytes).buffer.transferToImmutable());
}

const bytes = [1, 2, 3, 4, 5, 6, 7, 8];

// Writing into an immutable buffer throws and leaves the bytes unchanged.
for (const [string, encoding] of [
  ['ab', 'utf8'],
  ['ab', 'latin1'],
  ['ab', 'ascii'],
  ['ab', 'ucs2'],
  ['abcd', 'hex'],
  ['YWI=', 'base64'],
  ['YWI', 'base64url'],
]) {
  const buf = immutable(bytes);
  assert.throws(() => buf.write(string, encoding), immutableError);
  assert.throws(() => buf.write(string, 2, encoding), immutableError);
  assert.throws(() => buf[`${encoding}Write`](string), immutableError);
  assert.deepStrictEqual([...buf], bytes);
}

// Views into part of an immutable buffer are rejected too.
{
  const ab = Uint8Array.from(bytes).buffer.transferToImmutable();
  const view = Buffer.from(ab, 2, 4);
  assert.throws(() => view.write('ab'), immutableError);
  assert.deepStrictEqual([...new Uint8Array(ab)], bytes);
}

// Like TypedArray methods, a write of zero bytes is rejected as well.
{
  const buf = immutable(bytes);
  assert.throws(() => buf.write(''), immutableError);
  assert.throws(() => buf.write('ab', 8), immutableError);
}

// Invalid arguments are reported before the immutable backing store.
{
  const buf = immutable(bytes);
  assert.throws(() => buf.write(1), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => buf.write(1, 'hex'), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => buf.write('ab', 9), { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => buf.write('ab', 'nope'), { code: 'ERR_UNKNOWN_ENCODING' });
  assert.throws(() => buf.hexWrite('ab', 9),
                { code: 'ERR_BUFFER_OUT_OF_BOUNDS' });
  assert.deepStrictEqual([...buf], bytes);
}

// Mutable buffers are unaffected.
{
  const buf = Buffer.from(bytes);
  assert.strictEqual(buf.write('ab'), 2);
  assert.deepStrictEqual([...buf.subarray(0, 2)], [0x61, 0x62]);
}

// The optimized fast path rejects immutable buffers before writing.
{
  const buf = immutable(bytes);
  function write(target) {
    return utf8Write(target, 'ab', 0, 2);
  }
  eval('%PrepareFunctionForOptimization(utf8Write)');
  utf8Write(Buffer.alloc(8), 'ab', 0, 2);
  eval('%OptimizeFunctionOnNextCall(utf8Write)');
  assert.throws(() => write(buf), immutableError);
  assert.deepStrictEqual([...buf], bytes);

  if (common.isDebug) {
    const { getV8FastApiCallCount } = internalBinding('debug');
    assert.strictEqual(getV8FastApiCallCount('buffer.writeString'), 1);
  }
}
