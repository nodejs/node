'use strict';

// Tests that --process-timeout prints what kept the process running and exits
// with code 124.

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const {
  spawnSyncAndAssert,
  spawnSyncAndExit,
} = require('../common/child_process');

// The deadline is measured from the start of the process, so on a slow machine
// it can expire before the script has set up the state under test. The scripts
// write 'ready' to stdout once they have, and run again with a longer timeout
// if they did not get that far.
function runUntilReady(script) {
  for (let timeout = common.platformTimeout(1000); ; timeout *= 2) {
    const start = process.hrtime.bigint();
    const child = spawnSync(process.execPath, [
      `--process-timeout=${timeout}ms`,
      '-e',
      `const { writeSync } = require('fs');\n${script}`,
    ], { encoding: 'utf8' });
    const elapsed = Number(process.hrtime.bigint() - start) / 1e6;

    if (!child.stdout.startsWith('ready\n') &&
        timeout < common.platformTimeout(16000)) {
      continue;
    }

    assert.strictEqual(child.signal, null);
    assert.strictEqual(child.status, 124, child.stderr);
    // Neither 'beforeExit' nor 'exit' is emitted.
    assert.strictEqual(child.stdout, 'ready\n');
    assert.ok(elapsed >= timeout, `Exited after ${elapsed}ms, before ${timeout}ms`);
    assert.match(child.stderr, new RegExp(
      `^\\(node:\\d+\\) Process timed out after ${timeout}ms \\(--process-timeout\\)\\. ` +
      'Exiting with code 124\\.$', 'm'));
    return child.stderr;
  }
}

{
  // The event loop is kept alive by a server and a timer.
  const stderr = runUntilReady(`
    require('net').createServer().listen(0);
    setInterval(() => {}, 60_000);
    process.on('beforeExit', () => writeSync(1, 'beforeExit\\n'));
    process.on('exit', () => writeSync(1, 'exit\\n'));
    setImmediate(() => writeSync(1, 'ready\\n'));
  `);
  assert.match(stderr, /^Main thread was not executing JavaScript\.$/m);
  assert.match(stderr, /^Resources keeping the event loop alive:$/m);
  assert.match(stderr, /^ {4}TCPServerWrap \(listening on \S+:\d+[,)]/m);
  assert.match(stderr, /^ {4}Timeout \(next due in \d+ms\)$/m);
}

{
  // The main thread is executing JavaScript.
  const stderr = runUntilReady(`
    function spin() { for (;;); }
    writeSync(1, 'ready\\n');
    spin();
  `);
  assert.match(stderr,
               /^Main thread was executing JavaScript:\n {4}at spin \(\[eval\]:\d+:\d+\)$/m);
  assert.match(stderr,
               /^No resources keeping the event loop alive were found\.$/m);
}

{
  // The main thread is blocked in Atomics.wait(), which can be interrupted.
  const stderr = runUntilReady(`
    writeSync(1, 'ready\\n');
    Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0);
  `);
  assert.match(stderr, /^Main thread was executing JavaScript:\n {4}at \[eval\]:\d+:\d+$/m);
}

{
  // A Worker thread keeps the event loop alive.
  const stderr = runUntilReady(`
    const { Worker } = require('worker_threads');
    new Worker('setInterval(() => {}, 60_000)', { eval: true, name: 'poller' });
    writeSync(1, 'ready\\n');
  `);
  assert.match(stderr, /^ {4}Worker \(thread 1, name 'poller'\)$/m);
}

{
  // Processes that exit on their own are not affected.
  spawnSyncAndAssert(process.execPath, [
    '--process-timeout=10m',
    '-e',
    'setTimeout(() => console.log("done"), 1)',
  ], {
    stdout: 'done\n',
    stderr: '',
  });

  spawnSyncAndExit(process.execPath, [
    '--process-timeout=10m',
    '-e',
    'setTimeout(() => process.exit(3), 1)',
  ], {
    status: 3,
    signal: null,
    stderr: '',
  });
}
