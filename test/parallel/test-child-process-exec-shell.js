// Flags: --pending-deprecation
'use strict';
const common = require('../common');
const assert = require('assert');
const { exec, execSync } = require('child_process');

const invalidArgTypeError = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError'
};

for (const fn of [exec, execSync]) {
  assert.throws(() => fn('should-throw-on-boolean-shell-option', { shell: false }), invalidArgTypeError);
}

const deprecationMessage = 'Passing an empty string as the shell option when calling ' +
                           'child_process functions is deprecated.';
common.expectWarning('DeprecationWarning', deprecationMessage, 'DEP0195');
exec('does-not-exist', { shell: '' }, common.mustCall());
