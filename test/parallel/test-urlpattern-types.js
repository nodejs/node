'use strict';

require('../common');

const { URL, URLPattern } = require('url');
const assert = require('assert');

// Verifies that calling URLPattern with no new keyword throws.
assert.throws(() => URLPattern(), {
  code: 'ERR_CONSTRUCT_CALL_REQUIRED',
  name: 'TypeError',
});

// A primitive URLPatternInput is converted to USVString before parsing.
assert.throws(() => new URLPattern(1), {
  code: 'ERR_INVALID_URL_PATTERN',
  name: 'TypeError',
});

assert.throws(() => new URLPattern({}, 1), {
  code: 'ERR_INVALID_URL_PATTERN',
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

// Primitive input and baseURL values behave like their USVString conversions.
assert.deepStrictEqual(pattern.exec(1), pattern.exec('1'));
assert.deepStrictEqual(pattern.exec('', 1), pattern.exec('', '1'));
assert.strictEqual(pattern.test(1), pattern.test('1'));
assert.strictEqual(pattern.test('', 1), pattern.test('', '1'));

// Primitive URLPatternInput values select the USVString union branch.
{
  const baseURL = 'https://example/';
  const p = new URLPattern(123, baseURL);
  assert.strictEqual(p.pathname, '/123');

  const result = p.exec(123, baseURL);
  assert.notStrictEqual(result, null);
  assert.strictEqual(result.inputs[0], '123');
  assert.strictEqual(p.test(123, baseURL), true);
}

// Present URLPatternInit members are converted to USVString.
{
  const p = new URLPattern({ pathname: 123 });
  assert.strictEqual(p.pathname, '123');
  assert.strictEqual(p.test({ pathname: 123 }), true);
  assert.strictEqual(p.test({ pathname: 456 }), false);

  const result = p.exec({ pathname: 123 });
  assert.notStrictEqual(result, null);
  assert.strictEqual(result.inputs[0].pathname, '123');
}

// Only undefined URLPatternInit members are treated as absent.
{
  const undefinedPathname = new URLPattern({ pathname: undefined });
  assert.strictEqual(undefinedPathname.pathname, '*');

  const nullPathname = new URLPattern({ pathname: null });
  assert.strictEqual(nullPathname.pathname, 'null');
}

// URLPatternInit member conversion exceptions are propagated unchanged.
{
  const error = new Error('boom');
  const input = {
    pathname: {
      toString() {
        throw error;
      },
    },
  };
  const p = new URLPattern({ pathname: '123' });
  const isExpectedError = (actual) => actual === error;

  assert.throws(() => new URLPattern(input), isExpectedError);
  assert.throws(() => p.exec(input), isExpectedError);
  assert.throws(() => p.test(input), isExpectedError);
}

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

// Constructor: baseURL is converted to USVString after overload resolution.
{
  let calls = 0;
  const baseURL = {
    toString() {
      calls++;
      return 'https://example.com/';
    },
  };
  const p = new URLPattern('foo', baseURL, {});
  assert.strictEqual(calls, 1);
  assert.strictEqual(p.protocol, 'https');
  assert.strictEqual(p.hostname, 'example.com');
  assert.strictEqual(p.pathname, '/foo');
}

// exec() and test(): baseURL accepts string-convertible objects.
{
  const p = new URLPattern('https://example.com/foo');
  const baseURL = new URL('https://example.com/');
  assert.notStrictEqual(p.exec('foo', baseURL), null);
  assert.strictEqual(p.test('foo', baseURL), true);
}

// Exceptions thrown while converting baseURL are propagated unchanged.
{
  const error = new Error('boom');
  const baseURL = {
    toString() {
      throw error;
    },
  };
  const isExpectedError = (actual) => actual === error;

  assert.throws(
    () => new URLPattern('foo', baseURL, {}),
    isExpectedError,
  );
  assert.throws(() => pattern.exec('foo', baseURL), isExpectedError);
  assert.throws(() => pattern.test('foo', baseURL), isExpectedError);
}

// Symbol conversion throws the native TypeError required by USVString.
{
  const symbol = Symbol();
  const isUncodedTypeError = (error) =>
    error instanceof TypeError && error.code === undefined;

  assert.throws(
    () => new URLPattern(symbol, 'https://example/'),
    isUncodedTypeError,
  );
  assert.throws(
    () => pattern.exec(symbol, 'https://example/'),
    isUncodedTypeError,
  );
  assert.throws(
    () => pattern.test(symbol, 'https://example/'),
    isUncodedTypeError,
  );

  assert.throws(
    () => new URLPattern('foo', symbol, {}),
    isUncodedTypeError,
  );
  assert.throws(() => pattern.exec('foo', symbol), isUncodedTypeError);
  assert.throws(() => pattern.test('foo', symbol), isUncodedTypeError);
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
