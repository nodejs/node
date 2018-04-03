'use strict';
// Tests that a spawned child process can write to stdout without throwing.
// See https://github.com/nodejs/node-v0.x-archive/issues/1899.

require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const spawn = require('child_process').spawn;

const child = spawn(process.argv[0], [
  fixtures.path('GH-1899-output.js')
]);
let output = '';

child.stdout.on('data', function(data) {
  output += data;
});

child.on('exit', function(code, signal) {
  assert.strictEqual(code, 0);
  assert.strictEqual(output, 'hello, world!\n');
});
