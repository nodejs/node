'use strict';
require('../common');
const assert = require('assert');
const { validateHeaderName, validateHeaderValue } = require('http');

// Expected static methods
isFunc(validateHeaderName, 'validateHeaderName');
isFunc(validateHeaderValue, 'validateHeaderValue');

// Expected to be useful as static methods
console.log('validateHeaderName');
// - when used with valid header names - should not throw
[
  'user-agent',
  'USER-AGENT',
  'User-Agent',
  'x-forwarded-for',
].forEach((name) => {
  console.log('does not throw for "%s"', name);
  validateHeaderName(name);
});

// - when used with invalid header names:
[
  'איקס-פורוורד-פור',
  'x-forwarded-fםr',
].forEach((name) => {
  console.log('throws for: "%s"', name.slice(0, 50));
  assert.throws(
    () => validateHeaderName(name),
    { code: 'ERR_INVALID_HTTP_TOKEN' }
  );
});

console.log('validateHeaderValue');
// - when used with valid header values - should not throw
[
  ['x-valid', 1],
  ['x-valid', '1'],
  ['x-valid', 'string'],
].forEach(([name, value]) => {
  console.log('does not throw for "%s"', name);
  validateHeaderValue(name, value);
});

// - when used with invalid header values:
[
  // [header, value, expectedCode]
  ['x-undefined', undefined, 'ERR_HTTP_INVALID_HEADER_VALUE'],
  ['x-bad-char', 'לא תקין', 'ERR_INVALID_CHAR'],
].forEach(([name, value, code]) => {
  console.log('throws %s for: "%s: %s"', code, name, value);
  assert.throws(
    () => validateHeaderValue(name, value),
    { code }
  );
});

// Misc.
function isFunc(v, ttl) {
  assert.ok(v.constructor === Function, `${ttl} is expected to be a function`);
}
