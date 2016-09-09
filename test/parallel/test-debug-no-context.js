'use strict';

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

const args = ['--debug', `--debug-port=${common.PORT}`, '--interactive'];
const proc = spawn(process.execPath, args);
let commands = [
  'util.inspect(Promise.resolve(42));',
  'util.inspect(Promise.resolve(1337));',
  '.exit'
];
const run = ([cmd, ...rest]) => {
  if (!cmd) {
    return;
  }
  commands = rest;
  proc.stdin.write(`${cmd}\n`);
};

proc.on('exit', common.mustCall((exitCode, signalCode) => {
  // This next line should be included but unfortunately Win10 fails from time
  // to time in CI. See https://github.com/nodejs/node/issues/5268
  // assert.strictEqual(exitCode, 0);
  assert.strictEqual(signalCode, null);
}));
let stdout = '';
proc.stdout.setEncoding('utf8');
proc.stdout.on('data', (data) => {
  if (data !== '> ') {
    setTimeout(() => run(commands), 100);
  }
  stdout += data;
});
process.on('exit', () => {
  assert(stdout.includes('Promise { 42 }'));
  assert(stdout.includes('Promise { 1337 }'));
});

run(commands);
