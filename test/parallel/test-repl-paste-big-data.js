'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

// Pasting big input should not cause the process to have a huge CPU usage.

const cpuUsage = process.cpuUsage();

const { replServer } = startNewREPLServer({}, { disableDomainErrorAssert: true });
replServer.input.emit('data', 'a'.repeat(2e4) + '\n');
replServer.input.emit('data', '.exit\n');

replServer.once('exit', common.mustCall(() => {
  const diff = process.cpuUsage(cpuUsage);
  assert.ok(diff.user < 1e6);
}));
