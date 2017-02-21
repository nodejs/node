'use strict';
// This test ensures that flag --inspect-brk and --debug-brk works properly

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

const script = common.fixturesDir + '/empty.js';

function fail() {
  assert(0); // `node --debug-brk script.js` should not quit
}

if (cluster.isMaster) {

  function fork(execArgv) {
    cluster.setupMaster({execArgv});
    let worker = cluster.fork();

    worker.on('exit', fail);

    // give node time to start up the debugger
    setTimeout(() => {
      worker.removeListener('exit', fail);
      worker.kill();
    }, 2000);

    return worker;
  }
  
  let workers = [
    fork([script, '--inspect-brk']),
    fork([script, `--inspect-brk=5959`]),
    fork([script, '--debug-brk']),
    fork([script, `--debug-brk=9230`])
  ];

  process.on('exit', function() {
    workers.map((wk) => {
      assert.equal(wk.process.killed, true);
    });
  });
}
