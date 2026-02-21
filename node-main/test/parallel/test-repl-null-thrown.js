'use strict';
require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

const { replServer, output } = startNewREPLServer();

replServer.emit('line', 'process.nextTick(() => { throw null; })');
replServer.emit('line', '.exit');

setTimeout(() => {
  assert(output.accumulator.includes('Uncaught null'));
}, 0);
