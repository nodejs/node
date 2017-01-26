'use strict';
const common = require('../common');
const path = require('path');
const spawn = require('child_process').spawn;
const assert = require('assert');

const DELAY = common.platformTimeout(200);

const fixture = path.join(
  common.fixturesDir,
  'debugger-util-regression-fixture.js'
);

const args = [
  'debug',
  `--port=${common.PORT}`,
  fixture
];

const proc = spawn(process.execPath, args, { stdio: 'pipe' });
proc.stdout.setEncoding('utf8');
proc.stderr.setEncoding('utf8');

let stdout = '';
let stderr = '';
proc.stdout.on('data', (data) => stdout += data);
proc.stderr.on('data', (data) => stderr += data);

let nextCount = 0;
let exit = false;

// We look at output periodically. We don't do this in the on('data') as we
// may end up processing partial output. Processing periodically ensures that
// the debugger is in a stable state before we take the next step.
const timer = setInterval(() => {
  if (stdout.includes('> 1') && nextCount < 1 ||
      stdout.includes('> 2') && nextCount < 2 ||
      stdout.includes('> 3') && nextCount < 3 ||
      stdout.includes('> 4') && nextCount < 4) {
    nextCount++;
    proc.stdin.write('n\n');
  } else if (!exit && (stdout.includes('< { a: \'b\' }'))) {
    exit = true;
    proc.stdin.write('.exit\n');
    // We can cancel the timer and terminate normally.
    clearInterval(timer);
  } else if (stdout.includes('program terminated')) {
    // Catch edge case present in v4.x
    // process will terminate after call to util.inspect
    common.fail('the program should not terminate');
  }
}, DELAY);

process.on('exit', (code) => {
  assert.strictEqual(code, 0, 'the program should exit cleanly');
  assert.strictEqual(stdout.includes('{ a: \'b\' }'), true,
                     'the debugger should print the result of util.inspect');
  assert.strictEqual(stderr, '', 'stderr should be empty');
});
