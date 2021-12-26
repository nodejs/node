// Flags: --no-warnings
'use strict';

// This test checks that `_setSimultaneousAccepts` emit DEP0121 warning.
// Ref test-net-server-simultaneous-accepts-produce-warning-once.js

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
