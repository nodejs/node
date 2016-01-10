'use strict';
// Flags: --expose_internals

require('../common');
const assert = require('assert');
const I18N = require('internal/messages');

assert(I18N);
assert(I18N(I18N.ASSERTION_ERROR));
assert(I18N.messages);
assert(I18N.keys);
assert.equal(I18N.messages.length, I18N.keys.length);

const val = I18N(I18N.FUNCTION_REQUIRED, 'foo');
assert(/foo/.test(val));

assert.throws(() => {
  throw new I18N.Error(I18N.ASSERTION_ERROR);
}, Error);

assert.throws(() => {
  throw new I18N.TypeError(I18N.ASSERTION_ERROR);
}, TypeError);

assert.throws(() => {
  throw new I18N.RangeError(I18N.ASSERTION_ERROR);
}, RangeError);

assert(/ASSERTION_ERROR/.test(
  new I18N.Error(I18N.ASSERTION_ERROR).message));

// Verify the error properties
const err = new I18N.Error(I18N.ASSERTION_ERROR, 'foo');
assert.equal(err.key, I18N.keys[I18N.ASSERTION_ERROR]);
assert.equal(err.args.length, 1);
assert.equal(err.args[0], 'foo');
