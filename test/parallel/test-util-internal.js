'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { internalBinding } = require('internal/test/binding');

const {
  getHiddenValue,
  setHiddenValue,
  arrow_message_private_symbol: kArrowMessagePrivateSymbolIndex
} = internalBinding('util');

assert.strictEqual(
  getHiddenValue({}, kArrowMessagePrivateSymbolIndex),
  undefined);

const obj = {};
assert.strictEqual(
  setHiddenValue(obj, kArrowMessagePrivateSymbolIndex, 'bar'),
  true);
assert.strictEqual(
  getHiddenValue(obj, kArrowMessagePrivateSymbolIndex),
  'bar');

let arrowMessage;

try {
  require(fixtures.path('syntax', 'bad_syntax'));
} catch (err) {
  arrowMessage =
      getHiddenValue(err, kArrowMessagePrivateSymbolIndex);
}

assert(/bad_syntax\.js:1/.test(arrowMessage));
