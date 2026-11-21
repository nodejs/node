'use strict';
require('../common');
const assert = require('assert');
const {
  isValidHeaderName,
  isValidHeaderValue,
  validateHeaderName,
  validateHeaderValue,
} = require('http');

function succeeds(fn) {
  try {
    fn();
    return true;
  } catch {
    return false;
  }
}

// isValidHeaderName
{
  const valid = [
    'a',
    'user-agent',
    'USER-AGENT',
    'User-Agent',
    'x-forwarded-for',
    'x-request-id-with-a-long-name',
    "!#$%&'*+-.^_`|~",
    '0123456789',
  ];
  const invalid = [
    '',
    ' ',
    'bad header',
    'bad:header',
    'x-forwarded-fםr',
    'איקס-פורוורד-פור',
    'x\r\ny',
    'x\0',
    '(comment)',
    '"quoted"',
    'a,b',
    'long-invalid-header-name\u00e9',
  ];
  const nonStrings = [
    undefined, null, 0, 1, true, false, {}, [], ['a'],
    Symbol('a'), () => {}, 1n, Buffer.from('a'),
  ];

  for (const name of valid) {
    assert.strictEqual(isValidHeaderName(name), true, name);
  }
  for (const name of [...invalid, ...nonStrings]) {
    assert.strictEqual(isValidHeaderName(name), false, String(name?.toString?.()));
  }

  // Must agree with validateHeaderName() for every input.
  for (const name of [...valid, ...invalid, ...nonStrings]) {
    assert.strictEqual(
      isValidHeaderName(name),
      succeeds(() => validateHeaderName(name)),
    );
  }

  // Every single-character name agrees with validateHeaderName(), for both
  // the short (lookup table) and long (regexp) code paths.
  for (let c = 0; c <= 0x10ff; c++) {
    const ch = String.fromCharCode(c);
    for (const name of [ch, `${ch}xxxxxxxxxxxx`]) {
      assert.strictEqual(
        isValidHeaderName(name),
        succeeds(() => validateHeaderName(name)),
        `char code ${c}`,
      );
    }
  }
}

// isValidHeaderValue
{
  const valid = [
    '',
    'text/html',
    'a b\tc',
    '\u00e9\u00ff',
    '\x80',
    1,
    0,
    null,
    true,
    ['a', 'b'],
  ];
  const invalid = [
    undefined,
    'a\r\nb',
    'a\nb',
    'a\rb',
    'a\0b',
    'a\x01b',
    'a\x7fb',
    'לא תקין',
    '\u0100',
    ['a', 'b\n'],
    Symbol('a'),
  ];

  for (const value of valid) {
    assert.strictEqual(isValidHeaderValue(value), true, String(value));
  }
  for (const value of invalid) {
    assert.strictEqual(isValidHeaderValue(value), false, String(value));
  }

  // Must agree with validateHeaderValue() for every input.
  for (const value of [...valid, ...invalid]) {
    assert.strictEqual(
      isValidHeaderValue(value),
      succeeds(() => validateHeaderValue('x-test', value)),
    );
  }

  for (let c = 0; c <= 0x10ff; c++) {
    const value = `a${String.fromCharCode(c)}b`;
    assert.strictEqual(
      isValidHeaderValue(value),
      succeeds(() => validateHeaderValue('x-test', value)),
      `char code ${c}`,
    );
    // Explicit 'strict' is the same as the default.
    assert.strictEqual(
      isValidHeaderValue(value, { httpValidation: 'strict' }),
      isValidHeaderValue(value),
      `char code ${c}`,
    );
  }

  // 'relaxed' follows the Fetch spec: only NUL, CR, LF and code points above
  // U+00FF are rejected.
  const relaxed = { httpValidation: 'relaxed' };
  for (let c = 0; c <= 0x10ff; c++) {
    const expected = !(c === 0x00 || c === 0x0a || c === 0x0d || c > 0xff);
    assert.strictEqual(
      isValidHeaderValue(`a${String.fromCharCode(c)}b`, relaxed),
      expected,
      `char code ${c}`,
    );
  }
  assert.strictEqual(isValidHeaderValue(undefined, relaxed), false);
  assert.strictEqual(isValidHeaderValue('a\x01b', relaxed), true);
  assert.strictEqual(isValidHeaderValue('a\x7fb', relaxed), true);

  // An empty options object uses the default.
  assert.strictEqual(isValidHeaderValue('a\x01b', {}), false);
  assert.strictEqual(isValidHeaderValue('a\x01b', { httpValidation: undefined }), false);

  // Invalid options throw.
  for (const options of [null, 1, 'relaxed', true]) {
    assert.throws(() => isValidHeaderValue('a', options), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }
  for (const httpValidation of ['insecure', 'RELAXED', '', 1, null]) {
    assert.throws(() => isValidHeaderValue('a', { httpValidation }), {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  }
}
