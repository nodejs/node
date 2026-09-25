'use strict';

// Tests that --process-timeout makes the process exit even if the main thread
// cannot be interrupted.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// The deadline is measured from the start of the process, so on a slow machine
// it can expire before the fixture has reached the state under test, which the
// fixture signals by creating a file. If it did not get that far and did not
// print the expected message, run it again with a longer timeout.
function runUntilReady(fixture, expected) {
  for (let timeout = common.platformTimeout(1000); ; timeout *= 2) {
    // Child processes of a previous attempt may still be running, so use a
    // different file for each attempt.
    const marker = tmpdir.resolve(`${fixture}.${timeout}.ready`);
    const start = process.hrtime.bigint();
    const child = spawnSync(process.execPath, [
      `--process-timeout=${timeout}ms`,
      fixtures.path('process-timeout', fixture),
      marker,
    ], { encoding: 'utf8' });
    const elapsed = Number(process.hrtime.bigint() - start) / 1e6;

    if (!expected.test(child.stderr) && !fs.existsSync(marker) &&
        timeout < common.platformTimeout(16000)) {
      continue;
    }

    assert.strictEqual(child.signal, null);
    assert.strictEqual(child.status, 124, child.stderr);
    assert.ok(elapsed >= timeout, `Exited after ${elapsed}ms, before ${timeout}ms`);
    assert.match(child.stderr, new RegExp(
      `^\\(node:\\d+\\) Process timed out after ${timeout}ms \\(--process-timeout\\)\\. ` +
      'Exiting with code 124\\.$', 'm'));
    assert.match(child.stderr, expected);
    return child.stderr;
  }
}

{
  // The main thread is blocked in a synchronous native call.
  const stderr = runUntilReady(
    'blocked-main-thread.js',
    /^The main thread did not respond within 2000ms\. It is likely blocked/m);
  assert.doesNotMatch(stderr, /Main thread was/);
}

{
  // The main thread responds, but exiting waits for a Worker thread that is
  // blocked in a synchronous native call.
  const stderr = runUntilReady(
    'blocked-worker.js',
    /^\(node:\d+\) The process did not finish exiting within 5000ms after --process-timeout expired\. Forcing exit\.$/m);
  assert.match(stderr, /^ {4}Worker \(thread 1, name 'blocked'\)$/m);
}

{
  // The event loop has stopped, but exiting waits for a Worker thread that is
  // blocked in a synchronous native call.
  runUntilReady(
    'blocked-worker-at-exit.js',
    /^The process did not finish exiting after the event loop had stopped\.$/m);
}
