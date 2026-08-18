'use strict';
require('../common');
const { startNewREPLServer } = require('../common/repl');

// Note: This test ensures that async IIFE doesn't crash
// Ref: https://github.com/nodejs/node/issues/38685

const { replServer } = startNewREPLServer();

replServer.write('(async() => { })()\n');
replServer.write('.exit\n');
