'use strict';

const common = require('../common');

if (common.isWindows) {
  common.skip('POSIX signals are not supported on Windows');
}

const assert = require('node:assert');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const fs = require('node:fs');
const { describe, it } = require('node:test');

const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const node = JSON.stringify(process.execPath);
const forwardedSignals = ['SIGINT', 'SIGTERM'];

fs.writeFileSync(tmpdir.resolve('handle-signals.js'), `
const timer = setInterval(() => {}, 1000);
for (const signal of ${JSON.stringify(forwardedSignals)}) {
  process.on(signal, () => {
    console.log('received ' + signal);
    process.exitCode = 7;
    clearInterval(timer);
  });
}
console.log('ready', process.pid);
`);

fs.writeFileSync(tmpdir.resolve('package.json'), JSON.stringify({
  scripts: {
    'exit-code': 'exit 42',
    'self-terminate': 'kill -TERM $$',
    // `exec` so signals reach node, not the shell.
    'handle-signals': `exec ${node} handle-signals.js`,
    'no-signal-handlers':
      `exec ${node} -e "console.log('ready', process.pid); setInterval(() => {}, 1000)"`,
  },
}));

function run(script) {
  return common.spawnPromisified(process.execPath, ['--run', script], {
    cwd: tmpdir.path,
  });
}

// Sends `signal` to the runner once the script is ready.
async function runAndSignal(script, signal) {
  const child = spawn(process.execPath, ['--run', script], {
    cwd: tmpdir.path,
    stdio: ['ignore', 'pipe', 'inherit'],
  });
  const exited = once(child, 'exit');
  const closed = once(child, 'close');
  let stdout = '';
  let scriptPid;
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (chunk) => {
    stdout += chunk;
    const ready = scriptPid === undefined && /^ready (\d+)\n/.exec(stdout);
    if (ready) {
      scriptPid = Number(ready[1]);
      child.kill(signal);
    }
  });
  const [code, exitSignal] = await exited;
  // Don't leak the script if the runner didn't wait for it.
  if (scriptPid !== undefined) {
    try {
      process.kill(scriptPid, 'SIGKILL');
    } catch {
      // Already exited.
    }
  }
  await closed;
  return { code, signal: exitSignal, stdout };
}

describe('node --run signal handling', () => {
  it('uses the exit code of the script', async () => {
    const child = await run('exit-code');
    assert.strictEqual(child.code, 42);
    assert.strictEqual(child.signal, null);
  });

  it('terminates with the signal that terminated the script', async () => {
    const child = await run('self-terminate');
    assert.strictEqual(child.code, null);
    assert.strictEqual(child.signal, 'SIGTERM');
  });

  for (const signal of forwardedSignals) {
    it(`forwards ${signal} and waits for the script to exit`, async () => {
      const child = await runAndSignal('handle-signals', signal);
      assert.match(child.stdout, new RegExp(`^ready \\d+\\nreceived ${signal}\\n$`));
      assert.strictEqual(child.code, 7);
      assert.strictEqual(child.signal, null);
    });
  }

  it('terminates with a forwarded signal the script does not handle',
     async () => {
       const child = await runAndSignal('no-signal-handlers', 'SIGTERM');
       assert.strictEqual(child.code, null);
       assert.strictEqual(child.signal, 'SIGTERM');
     });
});
