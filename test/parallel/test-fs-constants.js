'use strict';
const { expectWarning } = require('../common');
const fs = require('fs');
const assert = require('assert');

// Check if the two constants accepted by chmod() on Windows are defined.
assert.notStrictEqual(fs.constants.S_IRUSR, undefined);
assert.notStrictEqual(fs.constants.S_IWUSR, undefined);

expectWarning(
  'DeprecationWarning',
  'fs.F_OK is deprecated, use fs.constants.F_OK instead',
  'DEP0176'
);
fs.F_OK;
fs.R_OK;
fs.W_OK;
fs.X_OK;
