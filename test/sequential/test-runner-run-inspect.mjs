import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

common.skipIfInspectorDisabled();


const debuggerPort = common.getPort();

async function spawnRunner({ execArgv, expectedPort, expectedHost, expectedInitialPort, inspectPort }) {
  const { code, signal } = await common.spawnPromisified(
    process.execPath,
    ['--expose-internals', '--no-warnings', ...execArgv, fixtures.path('test-runner/run_inspect.js')], {
      env: { ...process.env,
             expectedPort,
             inspectPort,
             expectedHost,
             expectedInitialPort }
    });
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

let offset = 0;

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
