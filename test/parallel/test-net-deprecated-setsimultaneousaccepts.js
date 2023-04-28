// Flags: --no-warnings
'use strict';

// Test that DEP0121 is emitted on the first call of _setSimultaneousAccepts().

const {
  expectWarning
} = require('../common');
const {
  _setSimultaneousAccepts
} = require('net');

expectWarning(
  'DeprecationWarning',
  'net._setSimultaneousAccepts() is deprecated and will be removed.',
  'DEP0121');

_setSimultaneousAccepts();
