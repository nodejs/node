// Flags: --no-warnings
// The flag suppresses stderr output but the warning event will still emit
'use strict';

const common = require('../common');
const assert = require('assert');

process.on('warning', common.mustCall((warning) => {
  assert(warning);
  assert(/^(Warning|CustomWarning)/.test(warning.name));
  assert(warning.message, 'A Warning');
}, 3));

process.emitWarning('A Warning');
process.emitWarning('A Warning', 'CustomWarning');

class CustomWarning extends Error {
  constructor() {
    super();
    this.name = 'CustomWarning';
    this.message = 'A Warning';
    Error.captureStackTrace(this, CustomWarning);
  }
}
process.emitWarning(new CustomWarning());

// TypeError is thrown on invalid output
assert.throws(() => process.emitWarning(1), TypeError);
assert.throws(() => process.emitWarning({}), TypeError);
