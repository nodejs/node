'use strict';
// Flags: --expose_internals

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const {
  getHiddenValue,
  setHiddenValue,
  arrow_message_private_symbol: kArrowMessagePrivateSymbolIndex,
  safeGetenv,
} = process.binding('util');

for (const oneEnv in process.env) {
  assert.strictEqual(
    safeGetenv(oneEnv),
    process.env[oneEnv]
  );
}

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
