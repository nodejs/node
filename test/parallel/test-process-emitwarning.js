// Flags: --no-warnings
// The flag suppresses stderr output but the warning event will still emit
'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');

process.on('warning', common.mustCall((warning) => {
  assert(warning);
  assert(/^(Warning|CustomWarning)/.test(warning.name));
  assert(warning.message, 'A Warning');
}, 7));

process.emitWarning('A Warning');
process.emitWarning('A Warning', 'CustomWarning');
process.emitWarning('A Warning', CustomWarning);
process.emitWarning('A Warning', 'CustomWarning', CustomWarning);

function CustomWarning() {
  Error.call(this);
  this.name = 'CustomWarning';
  this.message = 'A Warning';
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

// TypeError is thrown on invalid output
assert.throws(() => process.emitWarning(1), TypeError);
assert.throws(() => process.emitWarning({}), TypeError);
