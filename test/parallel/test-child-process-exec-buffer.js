'use strict';
require('../common');
var assert = require('assert');
var exec = require('child_process').exec;
var os = require('os');

var success_count = 0;

var str = 'hello';

// default encoding
var child = exec('echo ' + str, function(err, stdout, stderr) {
  assert.ok('string', typeof(stdout), 'Expected stdout to be a string');
  assert.ok('string', typeof(stderr), 'Expected stderr to be a string');
  assert.equal(str + os.EOL, stdout);

  success_count++;
});

// no encoding (Buffers expected)
var child = exec('echo ' + str, {
  encoding: null
}, function(err, stdout, stderr) {
  assert.ok(stdout instanceof Buffer, 'Expected stdout to be a Buffer');
  assert.ok(stderr instanceof Buffer, 'Expected stderr to be a Buffer');
  assert.equal(str + os.EOL, stdout.toString());

  success_count++;
});

process.on('exit', function() {
  assert.equal(2, success_count);
});
