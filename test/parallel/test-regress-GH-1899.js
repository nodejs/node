'use strict';
const common = require('../common');
var path = require('path');
var assert = require('assert');
var spawn = require('child_process').spawn;

var child = spawn(process.argv[0], [
  path.join(common.fixturesDir, 'GH-1899-output.js')
]);
var output = '';

child.stdout.on('data', function(data) {
  output += data;
});

child.on('exit', function(code, signal) {
  assert.equal(code, 0);
  assert.equal(output, 'hello, world!\n');
});

