'use strict';
// Flags: --inspect={PORT}
// This test ensures that flag --inspect-brk and --debug-brk works properly
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

const debuggerPort = common.PORT;
const script = common.fixturesDir + '/empty.js';

function fail() {
  assert(0); // `node --debug-brk script.js` should not quit
}

if (cluster.isMaster) {

  function fork(offset, execArgv) {
    cluster.setupMaster(execArgv);
    let worker = cluster.fork();

    worker.on('exit', fail);

    let socket = net.connect(debuggerPort + offset, () => {
      socket.end();
      worker.removeListener('exit', fail);
      worker.kill();
    });

    return worker;
  }
  
  assert.strictEqual(process.debugPort, debuggerPort);

  let workers = [
    fork(1, [`--inspect-brk`, script]),
    fork(2, [`--inspect-brk=${debuggerPort}`, script]),
    fork(3, [`--debug-brk`, script]),
    fork(4, [`--debug-brk=${debuggerPort}`, script])
  ];

  process.on('exit', function() {
    workers.map((wk) => {
      assert.equal(wk.process.killed, true);
    });
  });
}