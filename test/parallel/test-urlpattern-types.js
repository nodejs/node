'use strict';

require('../common');

const { URLPattern } = require('url');
const assert = require('assert');

// Verifies that calling URLPattern with no new keyword throws.
assert.throws(() => URLPattern(), {
  code: 'ERR_CONSTRUCT_CALL_REQUIRED',
  name: 'TypeError',
});

// Verifies that type checks are performed on the arguments.
assert.throws(() => new URLPattern(1), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});

assert.throws(() => new URLPattern({}, 1), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});

assert.throws(() => new URLPattern({}, '', 1), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});

// Per WebIDL, ignoreCase is coerced to boolean (not type-checked).
{
  const p = new URLPattern({}, { ignoreCase: '' });
  assert.strictEqual(p.protocol, '*');
}
{
  const p = new URLPattern({}, { ignoreCase: undefined });
  assert.strictEqual(p.protocol, '*');
}
{
  const p = new URLPattern({}, {});
  assert.strictEqual(p.protocol, '*');
}

const pattern = new URLPattern();

assert.throws(() => pattern.exec(1), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});

assert.throws(() => pattern.exec('', 1), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});

assert.throws(() => pattern.test(1), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});

assert.throws(() => pattern.test('', 1), {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});

// Per WebIDL, undefined/null for a URLPatternInput (union including dictionary)
// uses the default value (empty URLPatternInit {}).

// Constructor: undefined input should be treated as empty init.
{
  const p = new URLPattern(undefined);
  assert.strictEqual(p.protocol, '*');
  assert.strictEqual(p.hostname, '*');
}

// Constructor: null input should be treated as empty init (union, dict branch).
{
  const p = new URLPattern(null);
  assert.strictEqual(p.protocol, '*');
  assert.strictEqual(p.hostname, '*');
}

// Constructor: 2-arg with undefined/null uses overload 2 (options defaults).
{
  const p1 = new URLPattern(undefined, undefined);
  assert.strictEqual(p1.protocol, '*');
  const p2 = new URLPattern(null, null);
  assert.strictEqual(p2.protocol, '*');
  const p3 = new URLPattern({}, null);
  assert.strictEqual(p3.protocol, '*');
  const p4 = new URLPattern('https://example.com', null);
  assert.strictEqual(p4.hostname, 'example.com');
  const p5 = new URLPattern('https://example.com', undefined);
  assert.strictEqual(p5.hostname, 'example.com');
}

// Constructor: valid input with undefined/null options.
{
  const p = new URLPattern({ pathname: '/foo' }, undefined);
  assert.strictEqual(p.pathname, '/foo');
}

// Constructor: 3-arg with null/undefined baseURL is stringified per WebIDL,
// rejected as invalid URL by the parser.
assert.throws(
  () => new URLPattern('https://example.com', null, null),
  { code: 'ERR_INVALID_URL_PATTERN', name: 'TypeError' },
);
assert.throws(
  () => new URLPattern('https://example.com', undefined, undefined),
  { code: 'ERR_INVALID_URL_PATTERN', name: 'TypeError' },
);

// Constructor: 3-arg with valid baseURL and null options uses defaults.
{
  const p = new URLPattern('https://example.com', 'https://example.com', null);
  assert.strictEqual(p.hostname, 'example.com');
  const p2 = new URLPattern('https://example.com', 'https://example.com', undefined);
  assert.strictEqual(p2.hostname, 'example.com');
}

// exec() and test(): undefined input should be treated as empty init.
{
  const p = new URLPattern();
  assert.strictEqual(p.test(undefined), true);
  assert.strictEqual(p.test(undefined, undefined), true);
  assert.notStrictEqual(p.exec(undefined), null);
  assert.notStrictEqual(p.exec(undefined, undefined), null);
}

// exec() and test(): null input should be treated as empty init.
{
  const p = new URLPattern();
  assert.strictEqual(p.test(null), true);
  assert.notStrictEqual(p.exec(null), null);
}

// exec() and test(): null for baseURL is stringified to "null" per WebIDL.
// With string input, "null" is not a valid base URL so match fails silently.
// With dict input, providing baseURL with a dict throws per spec.
{
  const p = new URLPattern();
  // String input + null baseURL: no throw, match returns null (false).
  assert.strictEqual(p.test('https://example.com', null), false);
  assert.strictEqual(p.exec('https://example.com', null), null);
  // Dict input + null baseURL: throws (baseURL not allowed with dict input).
  assert.throws(() => p.test(null, null), {
    code: 'ERR_OPERATION_FAILED',
    name: 'TypeError',
  });
  assert.throws(() => p.exec(null, null), {
    code: 'ERR_OPERATION_FAILED',
    name: 'TypeError',
  });
}

// exec() and test(): valid input with undefined baseURL.
{
  const p = new URLPattern({ protocol: 'https' });
  assert.strictEqual(p.test('https://example.com', undefined), true);
  assert.notStrictEqual(p.exec('https://example.com', undefined), null);
}
