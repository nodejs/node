'use strict';

// Verifies that `--trace-sigterm` prints the JavaScript stack trace of a
// process that is stuck in an infinite loop, both when the process handles
// `SIGTERM` itself and when it does not.

const common = require('../common');

if (common.isWindows)
  common.skip('SIGTERM is not sent to processes on Windows');

const assert = require('assert');
const { spawn } = require('child_process');

const kMessage = 'TERMINATE: Script execution was interrupted by `SIGTERM`';
// The frames are written separately from the message above.
const kStackFrame = /^ {4}at .*test-trace-sigterm\.js:\d+/m;

if (process.argv[2] === 'child') {
  if (process.argv[3] === 'handled')
    process.on('SIGTERM', () => {});
  process.kill(process.pid, 'SIGTERM');
  while (true) {
    // Stuck, so that the trace has to come from an interrupt.
  }
}

function run(mode, expectedSignal) {
  const child = spawn(
    process.execPath,
    ['--trace-sigterm', __filename, 'child', mode],
    { stdio: ['ignore', 'ignore', 'pipe'] });

  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stderr.on('data', (chunk) => {
    stderr += chunk;
    // The handled case stays stuck, because its handler never gets to run.
    if (mode === 'handled' && kStackFrame.test(stderr))
      child.kill('SIGKILL');
  });

  child.on('exit', common.mustCall((code, signal) => {
    assert.ok(stderr.includes(kMessage), stderr);
    assert.match(stderr, kStackFrame);
    assert.strictEqual(code, null);
    assert.strictEqual(signal, expectedSignal);
  }));
}

// Without a handler, the signal terminates the process as usual.
run('unhandled', 'SIGTERM');

// With a handler, `--trace-sigterm` does not terminate the process, so it is
// killed above once the trace has been printed.
run('handled', 'SIGKILL');
