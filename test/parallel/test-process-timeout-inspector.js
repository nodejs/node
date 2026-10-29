'use strict';

// Tests that the inspector cannot be activated, and that sessions that could
// pause the main thread cannot be connected, while --process-timeout is used.

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawn } = require('child_process');
const { once } = require('events');
const inspector = require('inspector');
const readline = require('readline');
const { Worker } = require('worker_threads');
const { spawnSyncAndAssert } = require('../common/child_process');

async function testActivationRequestIsIgnored() {
  // Activation via SIGUSR1 (or its Windows equivalent) is ignored. The warning
  // is printed from another thread, so wait for it before checking whether the
  // inspector was activated.
  const child = spawn(process.execPath, [
    '--process-timeout=10m',
    '-e',
    `process._debugProcess(process.pid);
     process.stdin.once('data', () => {
       process.stdout.write(String(require('inspector').url()));
       process.stdin.destroy();
     });`,
  ]);
  let stdout = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (data) => { stdout += data; });
  const lines = readline.createInterface({ input: child.stderr });

  const [warning] = await once(lines, 'line');
  assert.match(warning,
               /^\(node:\d+\) Warning: Ignoring the request to activate the inspector because --process-timeout is used\.$/);
  child.stdin.end('check');

  const [code, signal] = await once(child, 'close');
  assert.strictEqual(signal, null);
  assert.strictEqual(code, 0);
  assert.strictEqual(stdout, 'undefined');
}

if (process.argv[2] !== 'child') {
  spawnSyncAndAssert(process.execPath, [
    '--process-timeout=10m',
    __filename,
    'child',
  ], {
    stderr: '',
  });

  testActivationRequestIsIgnored().then(common.mustCall());
  return;
}

assert.throws(() => inspector.open(0), {
  code: 'ERR_INSPECTOR_NOT_AVAILABLE',
  message: 'The inspector cannot be activated when --process-timeout is used',
});
assert.strictEqual(inspector.url(), undefined);

// Sessions on the same thread cannot keep the thread paused, so they work.
const session = new inspector.Session();
session.connect();
session.post('Runtime.evaluate', { expression: '1 + 2' }, common.mustSucceed(({ result }) => {
  assert.strictEqual(result.value, 3);
  session.disconnect();
}));

const worker = new Worker(`
  const assert = require('assert');
  const { Session } = require('inspector');
  assert.throws(() => new Session().connectToMainThread(), {
    code: 'ERR_INSPECTOR_NOT_AVAILABLE',
    message: 'Sessions cannot be connected to the main thread when ' +
             '--process-timeout is used',
  });
`, { eval: true });
worker.on('exit', common.mustCall((code) => assert.strictEqual(code, 0)));
