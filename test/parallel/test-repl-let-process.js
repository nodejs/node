'use strict';
require('../common');
const { startNewREPLServer } = require('../common/repl');

// Regression test for https://github.com/nodejs/node/issues/6802
const { input } = startNewREPLServer({ useGlobal: true });
input.run(['let process']);
