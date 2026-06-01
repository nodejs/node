'use strict';
require('../common');
const repl = require('repl');

// Regression test for https://github.com/nodejs/node/issues/63551:
// Typing a bare `import` keyword in the REPL must not crash the process.
const replserver = new repl.REPLServer();

replserver.emit('line', 'import');
replserver.emit('line', '.exit');
