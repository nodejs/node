// Flags: --no-warnings
'use strict';

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
