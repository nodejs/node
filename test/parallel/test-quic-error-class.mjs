// Flags: --experimental-quic --no-warnings

// Test: QuicError class. Validates the public surface of the new
// `QuicError` class exported from `node:quic`:
//   * Required `message` and `options.errorCode`.
//   * `errorCode` accepts bigint or number; coerces to bigint.
//   * `errorCode` is range-checked against the QUIC 62-bit varint
//     maximum.
//   * `type` defaults to 'application' and is restricted to
//     'application' | 'transport'.
//   * `code` defaults to 'ERR_QUIC_STREAM_ABORTED' and may be
//     overridden with any Node.js-style error code string.

import { hasQuic, skip } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, throws, ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { QuicError } = await import('node:quic');

// Sanity: QuicError is a function (class) and extends Error.
strictEqual(typeof QuicError, 'function');
ok(new QuicError('msg', { errorCode: 0n }) instanceof Error);

// Required arguments.
// `message` must be a string -> validateString throws ERR_INVALID_ARG_TYPE.
throws(() => new QuicError(), { code: 'ERR_INVALID_ARG_TYPE' });
// `options` defaults to an empty object, so `errorCode` is missing.
throws(() => new QuicError('msg'), { code: 'ERR_MISSING_ARGS' });
// Explicit non-object options -> validateObject throws ERR_INVALID_ARG_TYPE.
throws(() => new QuicError('msg', null), { code: 'ERR_INVALID_ARG_TYPE' });
// Empty options -> errorCode missing -> ERR_MISSING_ARGS.
throws(() => new QuicError('msg', {}), { code: 'ERR_MISSING_ARGS' });

// `message` must be a string.
throws(() => new QuicError(42, { errorCode: 0n }), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// `errorCode` must be bigint or number.
throws(() => new QuicError('msg', { errorCode: 'oops' }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
throws(() => new QuicError('msg', { errorCode: true }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
// `null` is preserved by destructuring (only `undefined` triggers the
// default), so it flows through to the type check rather than the
// missing-args check.
throws(() => new QuicError('msg', { errorCode: null }), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// `errorCode` range checks.
throws(() => new QuicError('msg', { errorCode: -1 }), {
  code: 'ERR_OUT_OF_RANGE',
});
throws(() => new QuicError('msg', { errorCode: -1n }), {
  code: 'ERR_OUT_OF_RANGE',
});
// 2**62 is the first invalid value (max varint is 2**62 - 1).
throws(() => new QuicError('msg', { errorCode: 1n << 62n }), {
  code: 'ERR_OUT_OF_RANGE',
});

// `type` validation.
throws(() => new QuicError('msg', { errorCode: 0n, type: 'bogus' }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
throws(() => new QuicError('msg', { errorCode: 0n, type: 42 }), {
  code: 'ERR_INVALID_ARG_VALUE',
});

// `code` (Node.js error code) must be a string when supplied.
throws(() => new QuicError('msg', { errorCode: 0n, code: 42 }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
throws(() => new QuicError('msg', { errorCode: 0n, code: null }), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// Happy paths.

// 1. Bigint errorCode, default type, default Node code.
{
  const err = new QuicError('something broke', { errorCode: 0x42n });
  strictEqual(err.message, 'something broke');
  strictEqual(err.code, 'ERR_QUIC_STREAM_ABORTED');
  strictEqual(err.errorCode, 0x42n);
  strictEqual(err.type, 'application');
  ok(err instanceof Error);
  ok(err instanceof QuicError);
}

// 2. Number errorCode, coerced to bigint.
{
  const err = new QuicError('numeric code', { errorCode: 5 });
  strictEqual(err.errorCode, 5n);
  strictEqual(typeof err.errorCode, 'bigint');
}

// 3. Boundary: 0n is allowed.
{
  const err = new QuicError('zero', { errorCode: 0n });
  strictEqual(err.errorCode, 0n);
}

// 4. Boundary: 2**62 - 1 is allowed (largest valid 62-bit varint).
{
  const max = (1n << 62n) - 1n;
  const err = new QuicError('max', { errorCode: max });
  strictEqual(err.errorCode, max);
}

// 5. Explicit type='transport'.
{
  const err = new QuicError('transport-level', {
    errorCode: 0x1n,
    type: 'transport',
  });
  strictEqual(err.type, 'transport');
}

// 6. Explicit type='application' (default).
{
  const err = new QuicError('app-level', {
    errorCode: 0x102n,
    type: 'application',
  });
  strictEqual(err.type, 'application');
}

// 7. Custom Node.js error code string via options.code.
{
  const err = new QuicError('custom', {
    errorCode: 0x10cn,
    code: 'ERR_CUSTOM_QUIC_FAILURE',
  });
  strictEqual(err.code, 'ERR_CUSTOM_QUIC_FAILURE');
  strictEqual(err.errorCode, 0x10cn);
}

// 8. Properties are read-only via getters (errorCode, type backed by
//    private fields). The base Error's message is writable but the
//    QuicError-specific accessors must not be assignable through the
//    prototype.
{
  const err = new QuicError('msg', { errorCode: 0x1n });
  // The getters live on the prototype; assigning through the instance
  // is silently ignored in non-strict mode (modules are strict, so
  // this throws TypeError). We just assert the value is unchanged.
  throws(() => { err.errorCode = 0xffn; }, { name: 'TypeError' });
  throws(() => { err.type = 'transport'; }, { name: 'TypeError' });
  strictEqual(err.errorCode, 0x1n);
  strictEqual(err.type, 'application');
}
