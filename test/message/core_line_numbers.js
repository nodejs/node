'use strict';
require('../common');
const path = require('path');

// This test verifies that line numbers in core modules are reported correctly.
// The path module was chosen for testing because it changes infrequently.
// If this test begins failing, it is likely due to a path update, and the
// test's assertions simply need to be updated to reflect the changes. If a
// path update was not made, and this test begins failing, then line numbers
// are probably actually broken.
path.dirname(0);
