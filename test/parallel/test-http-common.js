'use strict';
require('../common');
const assert = require('assert');
const {
  _checkInvalidHeaderChar: checkInvalidHeaderChar,
  _checkIsHttpToken: checkIsHttpToken,
  parsers
} = require('_http_common');

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

// parsers
{
  const dummyParser = { id: 'fhqwhgads' };
  assert.strictEqual(parsers.hasItems(), false);
  assert.strictEqual(parsers.free(dummyParser), true);
  assert.strictEqual(parsers.hasItems(), true);
  assert.strictEqual(parsers.alloc(), dummyParser);
  assert.strictEqual(parsers.hasItems(), false);
}
