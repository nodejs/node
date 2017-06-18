'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const cluster = require('cluster');

const debuggerPort = common.PORT;
const childProcess = require('child_process');

let offset = 0;

/*
 * This test suite checks that inspector port in cluster is incremented
 * for different execArgv combinations
 */

function testRunnerMain() {
  spawnMaster({
    execArgv: ['--inspect'],
    workers: [{expectedPort: 9230}]
  });

  let port = debuggerPort + offset++ * 5;

  spawnMaster({
    execArgv: [`--inspect=${port}`],
    workers: [
      {expectedPort: port + 1},
      {expectedPort: port + 2},
      {expectedPort: port + 3}
    ]
  });

  port = debuggerPort + offset++ * 5;

  spawnMaster({
    execArgv: ['--inspect', `--inspect-port=${port}`],
    workers: [{expectedPort: port + 1}]
  });

  port = debuggerPort + offset++ * 5;

  spawnMaster({
    execArgv: ['--inspect', `--debug-port=${port}`],
    workers: [{expectedPort: port + 1}]
  });

  port = debuggerPort + offset++ * 5;

  spawnMaster({
    execArgv: [`--inspect=0.0.0.0:${port}`],
    workers: [{expectedPort: port + 1, expectedHost: '0.0.0.0'}]
  });

  port = debuggerPort + offset++ * 5;

  spawnMaster({
    execArgv: [`--inspect=127.0.0.1:${port}`],
    workers: [{expectedPort: port + 1, expectedHost: '127.0.0.1'}]
  });

  if (common.hasIPv6) {
    port = debuggerPort + offset++ * 5;

    spawnMaster({
      execArgv: [`--inspect=[::]:${port}`],
      workers: [{expectedPort: port + 1, expectedHost: '::'}]
    });

    port = debuggerPort + offset++ * 5;

    spawnMaster({
      execArgv: [`--inspect=[::1]:${port}`],
      workers: [{expectedPort: port + 1, expectedHost: '::1'}]
    });
  }

  /*
   * Following tests check that port should not increment
   * if developer sets inspector port in cluster.setupMaster arguments.
   */

  port = debuggerPort + offset++ * 5;

  spawnMaster({
    execArgv: [`--inspect=${port}`],
    workers: [
      {expectedPort: port + 2},
      {expectedPort: port + 4},
      {expectedPort: port + 6}
    ],
    clusterExecArgv: [
      [`--inspect=${port + 2}`],
      [`--inspect-port=${port + 4}`],
      [`--debug-port=${port + 6}`],
    ]
  });

  port = debuggerPort + offset++ * 5;

  spawnMaster({
    execArgv: [],
    workers: [
      {expectedPort: port}
    ],
    clusterExecArgv: [
      [`--inspect=${port}`]
    ]
  });

  // Next tests check that inspector port incrementing logic
  // is disabled if zero port passed to workers.
  // Even if supplied execArgv is equal to master's.

  spawnMaster({
    execArgv: [],
    workers: [
      {expectedInitialPort: 0},
      {expectedInitialPort: 0},
      {expectedInitialPort: 0}
    ],
    clusterExecArgv: [
      ['--inspect=0'],
      ['--inspect=0'],
      ['--inspect=0']
    ]
  });

  spawnMaster({
    execArgv: ['--inspect=0'],
    workers: [
      {expectedInitialPort: 0},
      {expectedInitialPort: 0},
      {expectedInitialPort: 0}
    ],
    clusterExecArgv: [
      ['--inspect=0'],
      ['--inspect=0'],
      ['--inspect=0']
    ]
  });

  spawnMaster({
    execArgv: ['--inspect=0'],
    workers: [
      {expectedInitialPort: 0},
      {expectedInitialPort: 0},
      {expectedInitialPort: 0}
    ],
    clusterExecArgv: [
      ['--inspect', '--inspect-port=0'],
      ['--inspect', '--inspect-port=0'],
      ['--inspect', '--inspect-port=0']
    ]
  });

}

function masterProcessMain() {
  const workers = JSON.parse(process.env.workers);
  const clusterExecArgv = JSON.parse(process.env.clusterExecArgv);

  for (const [index, worker] of workers.entries()) {
    if (clusterExecArgv[index]) {
      cluster.setupMaster({execArgv: clusterExecArgv[index]});
    }

    cluster.fork({
      expectedPort: worker.expectedPort,
      expectedInitialPort: worker.expectedInitialPort,
      expectedHost: worker.expectedHost
    }).on('exit', common.mustCall(checkExitCode));
  }
}

function workerProcessMain() {
  const {expectedPort, expectedInitialPort, expectedHost} = process.env;
  const debugOptions = process.binding('config').debugOptions;

  if (+expectedPort) {
    assert.strictEqual(process.debugPort, +expectedPort);
  }

  if (+expectedInitialPort) {
    assert.strictEqual(debugOptions.port, +expectedInitialPort);
  }

  if (expectedHost !== 'undefined') {
    assert.strictEqual(debugOptions.host, expectedHost);
  }

  process.exit();
}

function spawnMaster({execArgv, workers, clusterExecArgv = []}) {
  childProcess.fork(__filename, {
    env: {
      workers: JSON.stringify(workers),
      clusterExecArgv: JSON.stringify(clusterExecArgv),
      testProcess: true
    },
    execArgv
  }).on('exit', common.mustCall(checkExitCode));
}

function checkExitCode(code, signal) {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

if (!process.env.testProcess) {
  testRunnerMain();
} else if (cluster.isMaster) {
  masterProcessMain();
} else {
  workerProcessMain();
}
