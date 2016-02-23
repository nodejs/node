'use strict';
require('../common');

// This test *only* verifies that invoking the stdin getter does not
// cause node to hang indefinitely.
// If it does, then the test-runner will nuke it.

// invoke the getter.
process.stdin;

console.error('Should exit normally now.');
