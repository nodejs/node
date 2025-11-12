'use strict';
require('../common');

// Regression test for https://github.com/nodejs/node/issues/30581
// This script should not crash.

function dummy() {}
process.on('SIGINT', dummy);
process.on('exit', () => process.removeListener('SIGINT', dummy));
