'use strict';

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

const args = ['--debug', `--debug-port=${common.PORT}`, '--interactive'];
const proc = spawn(process.execPath, args);
proc.stdin.write(`
    util.inspect(Promise.resolve(42));
    util.inspect(Promise.resolve(1337));
    .exit
`);
proc.on('exit', common.mustCall((exitCode, signalCode) => {
  // This next line should be included but unfortunately Win10 fails from time
  // to time in CI. See https://github.com/nodejs/node/issues/5268
  // assert.strictEqual(exitCode, 0);
  assert.strictEqual(signalCode, null);
}));
let stdout = '';
proc.stdout.setEncoding('utf8');
proc.stdout.on('data', (data) => stdout += data);
process.on('exit', () => {
  assert(stdout.includes('Promise { 42 }'));
  assert(stdout.includes('Promise { 1337 }'));
});
