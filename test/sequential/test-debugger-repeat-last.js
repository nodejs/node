'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const path = require('../common/fixtures').path;
const spawn = require('child_process').spawn;
const assert = require('assert');
const fixture = path('debugger-repeat-last.js');

const args = [
  'inspect',
  `--port=${common.PORT}`,
  fixture
];

const proc = spawn(process.execPath, args, { stdio: 'pipe' });
proc.stdout.setEncoding('utf8');

let stdout = '';

let sentCommand = false;
let sentExit = false;

proc.stdout.on('data', (data) => {
  stdout += data;

  // Send 'n' as the first step.
  if (!sentCommand && stdout.includes('> 1 ')) {
    setImmediate(() => { proc.stdin.write('n\n'); });
    return sentCommand = true;
  }
  // Send empty (repeat last command) until we reach line 5.
  if (sentCommand && !stdout.includes('> 5')) {
    setImmediate(() => { proc.stdin.write('\n'); });
    return true;
  }
  if (!sentExit && stdout.includes('> 5')) {
    setTimeout(() => { proc.stdin.write('\n\n\n.exit\n\n\n'); }, 1);
    return sentExit = true;
  }
});

process.on('exit', (exitCode) => {
  assert.strictEqual(exitCode, 0);
  console.log(stdout);
});
