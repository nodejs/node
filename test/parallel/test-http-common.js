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
assert(checkInvalidHeaderChar('あ'));
assert(checkInvalidHeaderChar('aaaaあaaaa'));

assert.strictEqual(checkInvalidHeaderChar(''), false);
assert.strictEqual(checkInvalidHeaderChar(1), false);
assert.strictEqual(checkInvalidHeaderChar(' '), false);
assert.strictEqual(checkInvalidHeaderChar(false), false);
assert.strictEqual(checkInvalidHeaderChar('t'), false);
assert.strictEqual(checkInvalidHeaderChar('tt'), false);
assert.strictEqual(checkInvalidHeaderChar('ttt'), false);
assert.strictEqual(checkInvalidHeaderChar('tttt'), false);
assert.strictEqual(checkInvalidHeaderChar('ttttt'), false);
