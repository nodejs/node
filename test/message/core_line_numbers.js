'use strict';
require('../common');
const punycode = require('punycode');

// This test verifies that line numbers in core modules are reported correctly.
// The punycode module was chosen for testing because it changes infrequently.
// If this test begins failing, it is likely due to a punycode update, and the
// test's assertions simply need to be updated to reflect the changes. If a
// punycode update was not made, and this test begins failing, then line numbers
// are probably actually broken.
punycode.decode('x');
