'use strict';

// Tests that --process-timeout-worker-stacks prints the JavaScript stacks of
// Worker threads, and what keeps their event loops alive, when
// --process-timeout expires.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// The deadline is measured from the start of the process, so on a slow machine
// it can expire before the Workers have been started. The script writes 'ready'
// to stdout once they have, and runs again with a longer timeout if it did not
// get that far.
function runUntilReady(args, isReady) {
  for (let timeout = common.platformTimeout(1000); ; timeout *= 2) {
    const child = spawnSync(process.execPath, [
      `--process-timeout=${timeout}ms`,
      ...args(timeout),
    ], { encoding: 'utf8' });
    if (!isReady(child, timeout) && timeout < common.platformTimeout(16000)) {
      continue;
    }
    assert.strictEqual(child.signal, null);
    assert.strictEqual(child.status, 124, child.stderr);
    return child.stderr;
  }
}

// One Worker is executing JavaScript, one is unref'd, and one is idle with a
// timer and has created a Worker of its own that is executing JavaScript.
const script = `
  const { writeSync } = require('fs');
  const { Worker } = require('worker_threads');
  const spin = 'require("worker_threads").parentPort.postMessage(0);' +
               'function spin() { for (;;); } spin();';
  let started = 0;
  function onStarted() {
    if (++started === 3) writeSync(1, 'ready\\n');
  }
  new Worker(spin, { eval: true, name: 'spinning' }).once('message', onStarted);
  new Worker(spin, { eval: true }).once('message', onStarted).unref();
  // Thread ids are allocated when a Worker is constructed, so this Worker is
  // created last. Otherwise the Worker it creates could be constructed before
  // the main thread has constructed its remaining Workers, and take one of
  // their thread ids.
  new Worker(\`
    const { parentPort, Worker } = require('worker_threads');
    new Worker(\${JSON.stringify(spin)}, { eval: true, name: 'nested' })
      .once('message', () => parentPort.postMessage(0));
    setTimeout(() => {}, 60_000);
  \`, { eval: true, name: 'idle' }).once('message', onStarted);
`;
const isReady = (child) => child.stdout === 'ready\n';

{
  const stderr = runUntilReady(
    () => ['--process-timeout-worker-stacks', '-e', script], isReady);
  assert.match(stderr, /^Main thread was not executing JavaScript\.$/m);
  assert.match(stderr, new RegExp(
    "^Worker thread 1 \\(name 'spinning'\\) was executing JavaScript:\\r?\\n" +
    ' {4}at spin \\(\\[worker eval\\]:1:\\d+\\)$', 'm'));
  // The resources keeping a Worker thread's event loop alive are listed after
  // its state.
  assert.match(stderr, new RegExp(
    "^Worker thread 3 \\(name 'idle'\\) was not executing JavaScript\\.\\r?\\n" +
    'Resources keeping its event loop alive:\\r?\\n' +
    ' {4}Timeout \\(next due in \\d+ms\\)\\r?\\n' +
    " {4}Worker \\(thread 4, name 'nested'\\)\\r?\\n" +
    '(?:Worker thread|$)', 'm'));
  assert.match(stderr,
               /^No resources keeping its event loop alive were found\.$/m);
  assert.match(stderr, new RegExp(
    "^Worker thread 4 \\(name 'nested'\\), created by Worker thread 3, was " +
    'executing JavaScript:\\r?\\n {4}at spin \\(\\[worker eval\\]:1:\\d+\\)$', 'm'));
  // Workers that do not keep the event loop alive are included too.
  assert.match(stderr, new RegExp(
    "^Worker thread 2 \\(name 'WorkerThread'\\) was executing JavaScript:\\r?\\n" +
    ' {4}at spin \\(\\[worker eval\\]:1:\\d+\\)$', 'm'));
  // The Workers are listed by thread id, after the main thread's summary.
  const order = [...stderr.matchAll(/^Worker thread (\d+)/mg)].map((m) => m[1]);
  assert.deepStrictEqual(order, ['1', '2', '3', '4']);
  assert.ok(stderr.indexOf('Worker thread 1') >
            stderr.indexOf('Resources keeping the event loop alive:'), stderr);
}

{
  // Without the option, the stacks of Worker threads are not printed.
  const stderr = runUntilReady(() => ['-e', script], isReady);
  assert.match(stderr, /^ {4}Worker \(thread 1, name 'spinning'\)$/m);
  assert.doesNotMatch(stderr, /^Worker thread/m);
}

{
  // A Worker thread that is blocked in a synchronous native call cannot
  // respond, so it is only waited for briefly. The fixture creates a file once
  // the Worker thread is blocked.
  const stderr = runUntilReady((timeout) => [
    '--process-timeout-worker-stacks',
    fixtures.path('process-timeout', 'blocked-worker.js'),
    // Child processes of a previous attempt may still be running, so use a
    // different file for each attempt.
    tmpdir.resolve(`blocked-worker.${timeout}.ready`),
  ], (child, timeout) =>
    fs.existsSync(tmpdir.resolve(`blocked-worker.${timeout}.ready`)));
  assert.match(stderr, new RegExp(
    "^Worker thread 1 \\(name 'blocked'\\) did not respond within 2000ms\\.\\r?\\n" +
    '\\(node:\\d+\\) The process did not finish exiting within 5000ms after ' +
    '--process-timeout expired\\. Forcing exit\\.$', 'm'));
}
