// Flags: --no-warnings
// The flag suppresses stderr output but the warning event will still emit
'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');

process.on('warning', common.mustCall((warning) => {
  assert(warning);
  assert(/^(Warning|CustomWarning)/.test(warning.name));
  assert.strictEqual(warning.message, 'A Warning');
  if (warning.code) assert.strictEqual(warning.code, 'CODE001');
}, 8));

process.emitWarning('A Warning');
process.emitWarning('A Warning', 'CustomWarning');
process.emitWarning('A Warning', CustomWarning);
process.emitWarning('A Warning', 'CustomWarning', CustomWarning);
process.emitWarning('A Warning', 'CustomWarning', 'CODE001');

function CustomWarning() {
  Error.call(this);
  this.name = 'CustomWarning';
  this.message = 'A Warning';
  this.code = 'CODE001';
  Error.captureStackTrace(this, CustomWarning);
}
util.inherits(CustomWarning, Error);
process.emitWarning(new CustomWarning());

const warningNoToString = new CustomWarning();
warningNoToString.toString = null;
process.emitWarning(warningNoToString);

const warningThrowToString = new CustomWarning();
warningThrowToString.toString = function() {
  throw new Error('invalid toString');
};
process.emitWarning(warningThrowToString);

const expectedError =
  common.expectsError({code: 'ERR_INVALID_ARG_TYPE', type: TypeError});

// TypeError is thrown on invalid input
assert.throws(() => process.emitWarning(1), expectedError);
assert.throws(() => process.emitWarning({}), expectedError);
assert.throws(() => process.emitWarning(true), expectedError);
assert.throws(() => process.emitWarning([]), expectedError);
assert.throws(() => process.emitWarning('', {}), expectedError);
assert.throws(() => process.emitWarning('', '', {}), expectedError);
assert.throws(() => process.emitWarning('', 1), expectedError);
assert.throws(() => process.emitWarning('', '', 1), expectedError);
assert.throws(() => process.emitWarning('', true), expectedError);
assert.throws(() => process.emitWarning('', '', true), expectedError);
assert.throws(() => process.emitWarning('', []), expectedError);
assert.throws(() => process.emitWarning('', '', []), expectedError);
