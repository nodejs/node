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
}

function masterProcessMain() {
  const workers = JSON.parse(process.env.workers);

  for (const worker of workers) {
    cluster.fork({
      expectedPort: worker.expectedPort,
      expectedHost: worker.expectedHost
    }).on('exit', common.mustCall(checkExitCode));
  }
}

function workerProcessMain() {
  const {expectedPort, expectedHost} = process.env;

  assert.strictEqual(process.debugPort, +expectedPort);

  if (expectedHost !== 'undefined') {
    assert.strictEqual(
      process.binding('config').debugOptions.host,
      expectedHost
    );
  }

  process.exit();
}

function spawnMaster({execArgv, workers}) {
  childProcess.fork(__filename, {
    env: {
      workers: JSON.stringify(workers),
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
