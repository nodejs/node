'use strict';
const common = require('../common');
const path = require('path');
const spawn = require('child_process').spawn;
const assert = require('assert');
const fixture = path.join(
  common.fixturesDir,
  'debugger-repeat-last.js'
);

const args = [
  'debug',
  `--port=${common.PORT}`,
  fixture
];

const proc = spawn(process.execPath, args, { stdio: 'pipe' });
proc.stdout.setEncoding('utf8');

var stdout = '';

var sentCommand = false;
var sentEmpty = false;
var sentExit = false;

proc.stdout.on('data', (data) => {
  stdout += data;
  if (!sentCommand && stdout.includes('> 1')) {
    setImmediate(() => {proc.stdin.write('n\n');});
    return sentCommand = true;
  }
  if (!sentEmpty && stdout.includes('> 3')) {
    setImmediate(() => {proc.stdin.write('\n');});
    return sentEmpty = true;
  }
  if (!sentExit && sentCommand && sentEmpty) {
    setTimeout(() => {proc.stdin.write('\n\n\n.exit\n\n\n');}, 1);
    return sentExit = true;
  }
});

process.on('exit', (exitCode) => {
  assert.strictEqual(exitCode, 0);
  console.log(stdout);
});
