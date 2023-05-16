'use strict';

// Test that DEP0121 is emitted only once if _setSimultaneousAccepts() is called
// more than once. This test is similar to
// test-net-deprecated-setsimultaneousaccepts.js, but that test calls
// _setSimultaneousAccepts() only once. Unlike this test, that will confirm
// that the warning is emitted on the first call. This test doesn't check which
// call caused the warning to be emitted.

const { expectWarning } = require('../common');
const { _setSimultaneousAccepts } = require('net');

expectWarning(
  'DeprecationWarning',
  'net._setSimultaneousAccepts() is deprecated and will be removed.',
  'DEP0121');

_setSimultaneousAccepts();
_setSimultaneousAccepts();
