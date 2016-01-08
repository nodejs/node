'use strict';
// Flags: --expose_internals

require('../common');
const assert = require('assert');
const I18N = require('internal/messages');

assert(I18N);
assert(I18N(I18N.NODE_ASSERTION_ERROR));

const val = I18N(I18N.NODE_FUNCTION_REQUIRED, 'foo');
assert(/foo/.test(val));

assert.throws(() => {
  throw I18N.Error(I18N.NODE_ASSERTION_ERROR);
}, Error);

assert.throws(() => {
  throw I18N.TypeError(I18N.NODE_ASSERTION_ERROR);
}, TypeError);

assert.throws(() => {
  throw I18N.RangeError(I18N.NODE_ASSERTION_ERROR);
}, RangeError);

assert(/NODE_ASSERTION_ERROR/.test(
  I18N.Error(I18N.NODE_ASSERTION_ERROR).message));
