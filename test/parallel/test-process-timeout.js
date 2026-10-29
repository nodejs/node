'use strict';

// Tests that --process-timeout prints what kept the process running and exits
// with code 124.

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
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
  // The event loop is kept alive by various kinds of handles.
  const file = JSON.stringify(fixtures.path('process-timeout', 'wait-for-parent.js'));
  const stderr = runUntilReady(`
    const { spawn } = require('child_process');
    const dgram = require('dgram');
    const { Resolver } = require('dns');
    const { once } = require('events');
    const fs = require('fs');
    const net = require('net');

    (async () => {
      const server = net.createServer().listen(0, '127.0.0.1');
      await once(server, 'listening');
      await once(net.connect(server.address().port, '127.0.0.1'), 'connect');

      // The DNS server never responds, so the query stays pending.
      const udp = dgram.createSocket('udp4').bind(0, '127.0.0.1');
      await once(udp, 'listening');
      const resolver = new Resolver({ timeout: 60_000, tries: 1 });
      resolver.setServers(['127.0.0.1:' + udp.address().port]);
      resolver.resolve4('example.org', () => {});

      const child = spawn(process.execPath, [${file}, String(process.pid)], {
        stdio: 'ignore',
      });
      await once(child, 'spawn');

      if (${!common.isIBMi}) fs.watch(${file});
      fs.watchFile(${file}, () => {});
      new MessageChannel().port1.on('message', () => {});
      writeSync(1, 'ready\\n');
    })();
  `);
  assert.match(stderr, /^Main thread was not executing JavaScript\.$/m);
  assert.match(stderr,
               /^ {4}TCPSocketWrap \(127\.0\.0\.1:\d+ -> 127\.0\.0\.1:\d+[,)]/m);
  assert.match(stderr, /^ {4}UDPWrap \(bound to 127\.0\.0\.1:\d+[,)]/m);
  // The DNS query keeps the event loop alive with handles of its own.
  assert.match(stderr, /^ {4}libuv handle(?: x\d+)? \(poll\)$/m);
  assert.match(stderr, /^ {4}ProcessWrap \(pid \d+\)$/m);
  if (!common.isIBMi) {
    assert.match(stderr, /^ {4}FSEventWrap \(watching .*wait-for-parent\.js\)$/m);
  }
  assert.match(stderr, /^ {4}StatWatcher \(watching .*wait-for-parent\.js\)$/m);
  assert.match(stderr, /^ {4}MessagePort$/m);
}

{
  // The main thread is executing JavaScript.
  const stderr = runUntilReady(`
    function spin() { for (;;); }
    writeSync(1, 'ready\\n');
    spin();
  `);
  // Native code writes to stderr in text mode, which ends lines with '\r\n' on
  // Windows.
  assert.match(stderr,
               /^Main thread was executing JavaScript:\r?\n {4}at spin \(\[eval\]:\d+:\d+\)$/m);
  assert.match(stderr,
               /^No resources keeping the event loop alive were found\.$/m);
}

{
  // Requests cannot complete while the main thread is executing JavaScript.
  // Resources that are indistinguishable from each other are counted.
  const stderr = runUntilReady(`
    const { stat } = require('fs');
    function spin() { for (;;); }
    stat(process.execPath, () => {});
    stat(process.execPath, () => {});
    writeSync(1, 'ready\\n');
    spin();
  `);
  assert.match(stderr,
               /^Main thread was executing JavaScript:\r?\n {4}at spin \(\[eval\]:\d+:\d+\)$/m);
  assert.match(stderr,
               /^Resources keeping the event loop alive:\r?\n {4}FSReqCallback x2$/m);
}

{
  // The main thread is executing a timer callback. The timer handle is inactive
  // while timers are being processed, so no due time is printed.
  const stderr = runUntilReady(`
    setTimeout(function spin() {
      writeSync(1, 'ready\\n');
      for (;;);
    }, 1);
  `);
  assert.match(stderr,
               /^Main thread was executing JavaScript:\r?\n {4}at spin \(\[eval\]:\d+:\d+\)$/m);
  assert.match(stderr, /^ {4}Timeout$/m);
}

{
  // The main thread is blocked in Atomics.wait(), which can be interrupted.
  const stderr = runUntilReady(`
    writeSync(1, 'ready\\n');
    Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0);
  `);
  assert.match(stderr, /^Main thread was executing JavaScript:\r?\n {4}at \[eval\]:\d+:\d+$/m);
}

{
  // A Worker thread keeps the event loop alive.
  const stderr = runUntilReady(`
    const { Worker } = require('worker_threads');
    new Worker('setInterval(() => {}, 60_000)', { eval: true, name: 'poller' });
    new Worker('setInterval(() => {}, 60_000)', { eval: true }).unref();
    writeSync(1, 'ready\\n');
  `);
  assert.match(stderr, /^ {4}Worker \(thread 1, name 'poller'\)$/m);
  // Workers that do not keep the event loop alive are not listed.
  assert.doesNotMatch(stderr, /^ {4}Worker \(thread 2\b/m);
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
