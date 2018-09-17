'use strict';

require('../common');

// This tests verifies that process.stdin.end() does not
// crash the process with ENOTCONN

process.stdin.end();
