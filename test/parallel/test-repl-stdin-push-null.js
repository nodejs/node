'use strict';
require('../common');

// This test ensures that the repl does not
// throw when using process.stdin stream to push null
// Refs: https://github.com/nodejs/node/issues/41330

const r = require('repl').start();

// Should not throw.
r.write('process.stdin.push(null)\n');
r.write('.exit\n');
