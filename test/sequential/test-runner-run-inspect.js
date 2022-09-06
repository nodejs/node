'use strict';

const common = require('../common');
const { parseArgs } = require('util');

common.skipIfInspectorDisabled();

const assert = require('assert');

const debuggerPort = common.PORT;
const childProcess = require('child_process');

const { values: { 'top-level': isTopLevel } } =
  parseArgs({ options: { topLevel: { type: 'boolean' } }, strict: false });

async function spawnRunner({ execArgv, expectedPort, expectedHost, expectedInitialPort, inspectPort }) {
  const { code, signal } = await new Promise((resolve) => {
    childProcess.spawn(process.execPath,
                       ['--expose-internals', '--no-warnings', ...execArgv, __filename, '--top-level'], {
                         env: { ...process.env,
                                expectedPort: JSON.stringify(expectedPort),
                                inspectPort,
                                expectedHost,
                                expectedInitialPort,
                                testProcess: true },
                         stdio: 'inherit',
                       }).on('exit', (code, signal) => resolve({ code, signal }));
  });
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

let offset = 0;

async function testRunnerMain() {
  const defaultPortCase = spawnRunner({
    execArgv: ['--inspect'],
    expectedPort: 9230,
  });

  await spawnRunner({
    execArgv: ['--inspect=65535'],
    expectedPort: 1024,
  });

  let port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: [`--inspect=${port}`],
    expectedPort: port + 1,
  });

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: ['--inspect', `--inspect-port=${port}`],
    expectedPort: port + 1,
  });

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: ['--inspect', `--debug-port=${port}`],
    expectedPort: port + 1,
  });

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: [`--inspect=0.0.0.0:${port}`],
    expectedPort: port + 1, expectedHost: '0.0.0.0',
  });

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: [`--inspect=127.0.0.1:${port}`],
    expectedPort: port + 1, expectedHost: '127.0.0.1'
  });

  if (common.hasIPv6) {
    port = debuggerPort + offset++ * 5;

    await spawnRunner({
      execArgv: [`--inspect=[::]:${port}`],
      expectedPort: port + 1, expectedHost: '::'
    });

    port = debuggerPort + offset++ * 5;

    await spawnRunner({
      execArgv: [`--inspect=[::1]:${port}`],
      expectedPort: port + 1, expectedHost: '::1'
    });
  }

  // These tests check that setting inspectPort in run
  // would take effect and override port incrementing behavior

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: [`--inspect=${port}`],
    inspectPort: port + 2,
    expectedPort: port + 2,
  });

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: [`--inspect=${port}`],
    inspectPort: 'addTwo',
    expectedPort: port + 2,
  });

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: [`--inspect=${port}`],
    inspectPort: 'string',
  });

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: [`--inspect=${port}`],
    inspectPort: 'null',
    expectedPort: port + 1,
  });

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: [`--inspect=${port}`],
    inspectPort: 'bignumber',
  });

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: [`--inspect=${port}`],
    inspectPort: 'negativenumber',
  });

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: [`--inspect=${port}`],
    inspectPort: 'bignumberfunc'
  });

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: [`--inspect=${port}`],
    inspectPort: 'strfunc',
  });

  port = debuggerPort + offset++ * 5;

  await spawnRunner({
    execArgv: [`--inspect=${port}`],
    inspectPort: 0,
    expectedInitialPort: 0,
  });

  await defaultPortCase;

  port = debuggerPort + offset++ * 5;
  await spawnRunner({
    execArgv: ['--inspect'],
    inspectPort: port + 2,
    expectedInitialPort: port + 2,
  });
}

const badPortError = { name: 'RangeError', code: 'ERR_SOCKET_BAD_PORT' };
function runTest() {
  const { run } = require('node:test');
  let inspectPort = 'inspectPort' in process.env ? Number(process.env.inspectPort) : undefined;
  let expectedError;

  if (process.env.inspectPort === 'addTwo') {
    inspectPort = common.mustCall(() => { return process.debugPort += 2; });
  } else if (process.env.inspectPort === 'string') {
    inspectPort = 'string';
    expectedError = badPortError;
  } else if (process.env.inspectPort === 'null') {
    inspectPort = null;
  } else if (process.env.inspectPort === 'bignumber') {
    inspectPort = 1293812;
    expectedError = badPortError;
  } else if (process.env.inspectPort === 'negativenumber') {
    inspectPort = -9776;
    expectedError = badPortError;
  } else if (process.env.inspectPort === 'bignumberfunc') {
    inspectPort = common.mustCall(() => 123121);
    expectedError = badPortError;
  } else if (process.env.inspectPort === 'strfunc') {
    inspectPort = common.mustCall(() => 'invalidPort');
    expectedError = badPortError;
  }

  const stream = run({ files: [__filename], inspectPort });
  if (expectedError) {
    stream.once('test:fail', common.mustCall(({ error }) => {
      assert.deepStrictEqual({ name: error.cause.name, code: error.cause.code }, expectedError);
    }));
  } else {
    stream.once('test:fail', ({ error }) => { throw error; });
  }
}


function assertParams() {
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

if (!process.env.testProcess && !isTopLevel) {
  testRunnerMain().then(common.mustCall());
} else if (isTopLevel) {
  runTest();
} else {
  assertParams();
}
