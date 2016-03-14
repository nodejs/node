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
}, 5));

process.emitWarning('A Warning');
process.emitWarning('A Warning', 'CustomWarning');

process.emitWarning(new Error('A Warning'));
process.emitWarning(new Error('A Warning'), 'CustomWarning');

function CustomWarning() {
  this.name = 'CustomWarning';
  this.message = 'A Warning';
  Error.captureStackTrace(this, CustomWarning);
}
util.inherits(CustomWarning, Error);
process.emitWarning(new CustomWarning());

// TypeError is thrown on invalid output
assert.throws(() => process.emitWarning(1), TypeError);
