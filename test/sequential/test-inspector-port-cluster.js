'use strict';

// Flags: --expose-internals

const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const cluster = require('cluster');

const debuggerPort = common.PORT;
const childProcess = require('child_process');

let offset = 0;

// This test suite checks that inspector port in cluster is incremented
// for different execArgv combinations

function testRunnerMain() {
  let defaultPortCase = spawnPrimary({
    execArgv: ['--inspect'],
    workers: [{ expectedPort: 9230 }]
  });

  spawnPrimary({
    execArgv: ['--inspect=65534'],
    workers: [
      { expectedPort: 65535 },
      { expectedPort: 1024 },
      { expectedPort: 1025 },
      { expectedPort: 1026 },
    ]
  });

  let port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [`--inspect=${port}`],
    workers: [
      { expectedPort: port + 1 },
      { expectedPort: port + 2 },
      { expectedPort: port + 3 },
    ]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: ['--inspect', `--inspect-port=${port}`],
    workers: [{ expectedPort: port + 1 }]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: ['--inspect', `--debug-port=${port}`],
    workers: [{ expectedPort: port + 1 }]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [`--inspect=0.0.0.0:${port}`],
    workers: [{ expectedPort: port + 1, expectedHost: '0.0.0.0' }]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [`--inspect=127.0.0.1:${port}`],
    workers: [{ expectedPort: port + 1, expectedHost: '127.0.0.1' }]
  });

  if (common.hasIPv6) {
    port = debuggerPort + offset++ * 5;

    spawnPrimary({
      execArgv: [`--inspect=[::]:${port}`],
      workers: [{ expectedPort: port + 1, expectedHost: '::' }]
    });

    port = debuggerPort + offset++ * 5;

    spawnPrimary({
      execArgv: [`--inspect=[::1]:${port}`],
      workers: [{ expectedPort: port + 1, expectedHost: '::1' }]
    });
  }

  // These tests check that setting inspectPort in cluster.settings
  // would take effect and override port incrementing behavior

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [`--inspect=${port}`],
    clusterSettings: { inspectPort: port + 2 },
    workers: [{ expectedPort: port + 2 }]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [`--inspect=${port}`],
    clusterSettings: { inspectPort: 'addTwo' },
    workers: [
      { expectedPort: port + 2 },
      { expectedPort: port + 4 },
    ]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [`--inspect=${port}`],
    clusterSettings: { inspectPort: 'string' },
    workers: [{}]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [`--inspect=${port}`],
    clusterSettings: { inspectPort: 'null' },
    workers: [{}]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [`--inspect=${port}`],
    clusterSettings: { inspectPort: 'bignumber' },
    workers: [{}]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [`--inspect=${port}`],
    clusterSettings: { inspectPort: 'negativenumber' },
    workers: [{}]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [`--inspect=${port}`],
    clusterSettings: { inspectPort: 'bignumberfunc' },
    workers: [{}]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [`--inspect=${port}`],
    clusterSettings: { inspectPort: 'strfunc' },
    workers: [{}]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [],
    clusterSettings: { inspectPort: port, execArgv: ['--inspect'] },
    workers: [
      { expectedPort: port },
    ]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [`--inspect=${port}`],
    clusterSettings: { inspectPort: 0 },
    workers: [
      { expectedInitialPort: 0 },
      { expectedInitialPort: 0 },
      { expectedInitialPort: 0 },
    ]
  });

  port = debuggerPort + offset++ * 5;

  spawnPrimary({
    execArgv: [],
    clusterSettings: { inspectPort: 0 },
    workers: [
      { expectedInitialPort: 0 },
      { expectedInitialPort: 0 },
      { expectedInitialPort: 0 },
    ]
  });

  defaultPortCase.then(() => {
    port = debuggerPort + offset++ * 5;
    defaultPortCase = spawnPrimary({
      execArgv: ['--inspect'],
      clusterSettings: { inspectPort: port + 2 },
      workers: [
        { expectedInitialPort: port + 2 },
      ]
    });
  });
}

function primaryProcessMain() {
  const workers = JSON.parse(process.env.workers);
  const clusterSettings = JSON.parse(process.env.clusterSettings) || {};
  const badPortError = { name: 'RangeError', code: 'ERR_SOCKET_BAD_PORT' };
  let debugPort = process.debugPort;

  for (const worker of workers) {
    const params = {};

    if (worker.expectedPort) {
      params.expectedPort = worker.expectedPort;
    }

    if (worker.expectedInitialPort) {
      params.expectedInitialPort = worker.expectedInitialPort;
    }

    if (worker.expectedHost) {
      params.expectedHost = worker.expectedHost;
    }

    clusterSettings.execArgv = clusterSettings.execArgv ?
      clusterSettings.execArgv.concat(['--expose-internals']) :
      process.execArgv.concat(['--expose-internals']);

    if (clusterSettings.inspectPort === 'addTwo') {
      clusterSettings.inspectPort = common.mustCall(
        () => { return debugPort += 2; },
        workers.length
      );
    } else if (clusterSettings.inspectPort === 'string') {
      clusterSettings.inspectPort = 'string';
      cluster.setupPrimary(clusterSettings);

      assert.throws(() => {
        cluster.fork(params).on('exit', common.mustCall(checkExitCode));
      }, badPortError);

      return;
    } else if (clusterSettings.inspectPort === 'null') {
      clusterSettings.inspectPort = null;
      cluster.setupPrimary(clusterSettings);

      assert.throws(() => {
        cluster.fork(params).on('exit', common.mustCall(checkExitCode));
      }, badPortError);

      return;
    } else if (clusterSettings.inspectPort === 'bignumber') {
      clusterSettings.inspectPort = 1293812;
      cluster.setupPrimary(clusterSettings);

      assert.throws(() => {
        cluster.fork(params).on('exit', common.mustCall(checkExitCode));
      }, badPortError);

      return;
    } else if (clusterSettings.inspectPort === 'negativenumber') {
      clusterSettings.inspectPort = -9776;
      cluster.setupPrimary(clusterSettings);

      assert.throws(() => {
        cluster.fork(params).on('exit', common.mustCall(checkExitCode));
      }, badPortError);

      return;
    } else if (clusterSettings.inspectPort === 'bignumberfunc') {
      clusterSettings.inspectPort = common.mustCall(
        () => 123121,
        workers.length
      );

      cluster.setupPrimary(clusterSettings);

      assert.throws(() => {
        cluster.fork(params).on('exit', common.mustCall(checkExitCode));
      }, badPortError);

      return;
    } else if (clusterSettings.inspectPort === 'strfunc') {
      clusterSettings.inspectPort = common.mustCall(
        () => 'invalidPort',
        workers.length
      );

      cluster.setupPrimary(clusterSettings);

      assert.throws(() => {
        cluster.fork(params).on('exit', common.mustCall(checkExitCode));
      }, badPortError);

      return;
    }

    cluster.setupPrimary(clusterSettings);

    cluster.fork(params).on('exit', common.mustCall(checkExitCode));
  }
}

function workerProcessMain() {
  const { expectedPort, expectedInitialPort, expectedHost } = process.env;
  const debugOptions =
    require('internal/options').getOptionValue('--inspect-port');

  if ('expectedPort' in process.env) {
    assert.strictEqual(process.debugPort, +expectedPort);
  }

  if ('expectedInitialPort' in process.env) {
    assert.strictEqual(debugOptions.port, +expectedInitialPort);
  }

  if ('expectedHost' in process.env) {
    assert.strictEqual(debugOptions.host, expectedHost);
  }

  process.exit();
}

function spawnPrimary({ execArgv, workers, clusterSettings = {} }) {
  return new Promise((resolve) => {
    childProcess.fork(__filename, {
      env: { ...process.env,
             workers: JSON.stringify(workers),
             clusterSettings: JSON.stringify(clusterSettings),
             testProcess: true },
      execArgv: execArgv.concat(['--expose-internals'])
    }).on('exit', common.mustCall((code, signal) => {
      checkExitCode(code, signal);
      resolve();
    }));
  });
}

function checkExitCode(code, signal) {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

if (!process.env.testProcess) {
  testRunnerMain();
} else if (cluster.isPrimary) {
  primaryProcessMain();
} else {
  workerProcessMain();
}
