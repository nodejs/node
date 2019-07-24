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
    { char: 'あ', index: 2 }
);

assert.strictEqual(checkInvalidHeaderChar(''), null);
assert.strictEqual(checkInvalidHeaderChar(1), null);
assert.strictEqual(checkInvalidHeaderChar(' '), null);
assert.strictEqual(checkInvalidHeaderChar(false), null);
assert.strictEqual(checkInvalidHeaderChar('t'), null);
assert.strictEqual(checkInvalidHeaderChar('tt'), null);
assert.strictEqual(checkInvalidHeaderChar('ttt'), null);
assert.strictEqual(checkInvalidHeaderChar('tttt'), null);
assert.strictEqual(checkInvalidHeaderChar('ttttt'), null);
