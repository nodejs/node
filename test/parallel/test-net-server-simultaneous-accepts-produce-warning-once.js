'use strict';

// This test checks that `_setSimultaneousAccepts` does not emit DEP0121 warning
// in second calling, by combination with
// `test-net-deprecated-setsimultaneousaccepts.js`.

const { expectWarning } = require('../common');
const { _setSimultaneousAccepts } = require('net');

expectWarning(
  'DeprecationWarning',
  'net._setSimultaneousAccepts() is deprecated and will be removed.',
  'DEP0121');

_setSimultaneousAccepts();
_setSimultaneousAccepts();
