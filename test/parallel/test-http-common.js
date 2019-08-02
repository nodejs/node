'use strict';
require('../common');
const assert = require('assert');
const httpCommon = require('_http_common');
const checkIsHttpToken = httpCommon._checkIsHttpToken;
const checkInvalidHeaderChar = httpCommon._checkInvalidHeaderChar;

// checkIsHttpToken
assert(checkIsHttpToken('t'));
assert(checkIsHttpToken('tt'));
assert(checkIsHttpToken('ttt'));
assert(checkIsHttpToken('tttt'));
assert(checkIsHttpToken('ttttt'));

assert.strictEqual(checkIsHttpToken(''), false);
assert.strictEqual(checkIsHttpToken(' '), false);
assert.strictEqual(checkIsHttpToken('あ'), false);
assert.strictEqual(checkIsHttpToken('あa'), false);
assert.strictEqual(checkIsHttpToken('aaaaあaaaa'), false);

// checkInvalidHeaderChar
assert.deepStrictEqual(checkInvalidHeaderChar('あ'), { char: 'あ', index: 0 });
assert.deepStrictEqual(
  checkInvalidHeaderChar('aaaaあaaaa'),
  { char: 'あ', index: 4 }
);

assert.strictEqual(checkInvalidHeaderChar(''), undefined);
assert.strictEqual(checkInvalidHeaderChar(1), undefined);
assert.strictEqual(checkInvalidHeaderChar(' '), undefined);
assert.strictEqual(checkInvalidHeaderChar(false), undefined);
assert.strictEqual(checkInvalidHeaderChar('t'), undefined);
assert.strictEqual(checkInvalidHeaderChar('tt'), undefined);
assert.strictEqual(checkInvalidHeaderChar('ttt'), undefined);
assert.strictEqual(checkInvalidHeaderChar('tttt'), undefined);
assert.strictEqual(checkInvalidHeaderChar('ttttt'), undefined);
