'use strict';
const assert = require('assert');
const { exec, execSync } = require('child_process');

const invalidArgTypeError = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError'
};

for (const fn of [exec, execSync]) {
  assert.throws(() => fn('should-throw-on-boolean-shell-option', { shell: false }), invalidArgTypeError);
}
