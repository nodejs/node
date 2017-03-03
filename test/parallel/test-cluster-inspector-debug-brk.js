'use strict';
// Flags: --inspect={PORT}
// This test ensures that flag --inspect-brk and --debug-brk works properly
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

const debuggerPort = common.PORT;
const script = common.fixturesDir + '/empty.js';

if (cluster.isMaster) {

  function fork(offset, execArgv) {
    cluster.setupMaster(execArgv);
    const worker = cluster.fork();

    worker.on('exit', common.mustCall((code, signal) => {
      assert.strictEqual(code, null);
      assert.strictEqual(signal, 'SIGTERM');
    }));

    worker.on('online', common.mustCall(() => {
      const socket = net.connect(debuggerPort + offset, common.mustCall(() => {
        socket.end();
        worker.kill();
      }));
    }));
  }

  assert.strictEqual(process.debugPort, debuggerPort);

  fork(1, ['--inspect-brk', script]);
  fork(2, [`--inspect-brk=${debuggerPort}`, script]);
  fork(3, ['--debug-brk', script]);
  fork(4, [`--debug-brk=${debuggerPort}`, script]);
}
