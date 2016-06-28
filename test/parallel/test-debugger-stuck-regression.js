'use strict';
const path = require('path');
const spawn = require('child_process').spawn;
const assert = require('assert');

const common = require('../common');

const fixture = path.join(
  common.fixturesDir,
  'debugger-stuck-regression-fixture.js'
);

const args = [
  'debug',
  `--port=${common.PORT}`,
  fixture
];

const TEST_TIMEOUT_MS = common.platformTimeout(4000);

function onTestTimeout() {
  common.fail('The debuggee did not terminate.');
}

const testTimeout = setTimeout(onTestTimeout, TEST_TIMEOUT_MS);

const proc = spawn(process.execPath, args, { stdio: 'pipe' });
proc.stdout.setEncoding('utf8');
proc.stderr.setEncoding('utf8');

const STATE_WAIT_BREAK = 0;
const STATE_WAIT_PROC_TERMINATED = 1;
const STATE_EXIT = 2;

let stdout = '';
let stderr = '';
let state = 0;
let cycles = 2;

proc.stdout.on('data', (data) => {
  stdout += data;

  switch (state) {
    case STATE_WAIT_BREAK:
      // check if the debugger stopped at line 1
      if (stdout.includes('> 1')) {
        stdout = '';

        // send continue to the debugger
        proc.stdin.write('c\n');
        state = STATE_WAIT_PROC_TERMINATED;
      }
      break;
    case STATE_WAIT_PROC_TERMINATED:
      // check if the debuggee has terminated
      if (stdout.includes('program terminated')) {
        stdout = '';

        // If cycles is greater than 0,
        // we re-run the debuggee.
        // Otherwise exit the debugger.
        if (cycles > 0) {
          --cycles;
          state = STATE_WAIT_BREAK;
          proc.stdin.write('run\n');
        } else {
          // cancel the test timeout
          clearTimeout(testTimeout);
          state = STATE_EXIT;
          proc.stdin.write('.exit\n');
        }

      }
      break;
    case STATE_EXIT:
      // fall through
    default:
      break;
  }
});

proc.stderr.on('data', (data) => {
  stderr += data;
});

proc.stdin.on('error', (err) => {
  console.error(err);
});

process.on('exit', (code) => {
  assert.equal(code, 0, 'the program should exit cleanly');
  assert.equal(stderr, '', 'stderr should be empty');
});
