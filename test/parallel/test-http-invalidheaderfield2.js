'use strict';
require('../common');
const assert = require('assert');
const inspect = require('util').inspect;
const { _checkIsHttpToken, _checkInvalidHeaderChar } = require('_http_common');

// Good header field names
[
  'TCN',
  'ETag',
  'date',
  'alt-svc',
  'Content-Type',
  '0',
  'Set-Cookie2',
  'Set_Cookie',
  'foo`bar^',
  'foo|bar',
  '~foobar',
  'FooBar!',
  '#Foo',
  '$et-Cookie',
  '%%Test%%',
  'Test&123',
  'It\'s_fun',
  '2*3',
  '4+2',
  '3.14159265359',
].forEach(function(str) {
  assert.strictEqual(
    _checkIsHttpToken(str), true,
    `_checkIsHttpToken(${inspect(str)}) unexpectedly failed`);
});
// Bad header field names
[
  ':',
  '@@',
  '中文呢', // unicode
  '((((())))',
  ':alternate-protocol',
  'alternate-protocol:',
  'foo\nbar',
  'foo\rbar',
  'foo\r\nbar',
  'foo\x00bar',
  '\x7FMe!',
  '{Start',
  '(Start',
  '[Start',
  'End}',
  'End)',
  'End]',
  '"Quote"',
  'This,That',
].forEach(function(str) {
  assert.strictEqual(
    _checkIsHttpToken(str), false,
    `_checkIsHttpToken(${inspect(str)}) unexpectedly succeeded`);
});


// ============================================================================
// Strict header value validation (default) - per RFC 7230
// Rejects control characters (0x00-0x1f except HTAB) and DEL (0x7f)
// ============================================================================

// Good header field values in strict mode
[
  'foo bar',
  'foo\tbar',  // HTAB is allowed
  '0123456789ABCdef',
  '!@#$%^&*()-_=+\\;\':"[]{}<>,./?|~`',
  '\x80\x81\xff',  // obs-text (0x80-0xff) is allowed
].forEach(function(str) {
  assert.strictEqual(
    _checkInvalidHeaderChar(str), false,
    `_checkInvalidHeaderChar(${inspect(str)}) unexpectedly failed in strict mode`);
});

// Bad header field values in strict mode
// Control characters (except HTAB) and DEL are rejected
[
  'foo\x00bar',     // NUL
  'foo\x01bar',     // SOH
  'foo\rbar',       // CR
  'foo\nbar',       // LF
  'foo\r\nbar',     // CRLF
  'foo\x7Fbar',     // DEL
  '中文呢',          // unicode > 0xff
].forEach(function(str) {
  assert.strictEqual(
    _checkInvalidHeaderChar(str), true,
    `_checkInvalidHeaderChar(${inspect(str)}) unexpectedly succeeded in strict mode`);
});


// ============================================================================
// Lenient header value validation (with insecureHTTPParser) - per Fetch spec
// Only NUL (0x00), CR (0x0d), LF (0x0a), and chars > 0xff are rejected
// ============================================================================

// Good header field values in lenient mode
// CTL characters (except NUL, LF, CR) are valid per Fetch spec
[
  'foo bar',
  'foo\tbar',
  '0123456789ABCdef',
  '!@#$%^&*()-_=+\\;\':"[]{}<>,./?|~`',
  '\x01\x02\x03\x04\x05\x06\x07\x08',  // 0x01-0x08
  'foo\x0bbar',                         // VT (0x0b)
  'foo\x0cbar',                         // FF (0x0c)
  '\x0e\x0f\x10\x11\x12\x13\x14\x15',  // 0x0e-0x15
  '\x16\x17\x18\x19\x1a\x1b\x1c\x1d',  // 0x16-0x1d
  '\x1e\x1f',                           // 0x1e-0x1f
  '\x7FMe!',                            // DEL (0x7f)
  '\x80\x81\xff',                       // obs-text (0x80-0xff)
].forEach(function(str) {
  assert.strictEqual(
    _checkInvalidHeaderChar(str, true), false,
    `_checkInvalidHeaderChar(${inspect(str)}, true) unexpectedly failed in lenient mode`);
});

// Bad header field values in lenient mode
// Only NUL (0x00), LF (0x0a), CR (0x0d), and characters > 0xff are invalid
[
  'foo\rbar',         // CR (0x0d)
  'foo\nbar',         // LF (0x0a)
  'foo\r\nbar',       // CRLF
  '中文呢',           // unicode > 0xff
  'Testing 123\x00',  // NUL (0x00)
].forEach(function(str) {
  assert.strictEqual(
    _checkInvalidHeaderChar(str, true), true,
    `_checkInvalidHeaderChar(${inspect(str)}, true) unexpectedly succeeded in lenient mode`);
});
