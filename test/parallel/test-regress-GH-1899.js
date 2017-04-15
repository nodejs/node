'use strict';
const common = require('../common');
const path = require('path');
const assert = require('assert');
const spawn = require('child_process').spawn;

const child = spawn(process.argv[0], [
  path.join(common.fixturesDir, 'GH-1899-output.js')
]);
let output = '';

child.stdout.on('data', function(data) {
  output += data;
});

child.on('exit', function(code, signal) {
  assert.strictEqual(code, 0);
  assert.strictEqual(output, 'hello, world!\n');
});
