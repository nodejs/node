'use strict';
const common = require('../common');
var assert = require('assert');
var exec = require('child_process').exec;
var os = require('os');
var str = 'hello';

// default encoding
exec('echo ' + str, common.mustCall(function(err, stdout, stderr) {
  assert.ok('string', typeof stdout, 'Expected stdout to be a string');
  assert.ok('string', typeof stderr, 'Expected stderr to be a string');
  assert.equal(str + os.EOL, stdout);
}));

// no encoding (Buffers expected)
exec('echo ' + str, {
  encoding: null
}, common.mustCall(function(err, stdout, stderr) {
  assert.ok(stdout instanceof Buffer, 'Expected stdout to be a Buffer');
  assert.ok(stderr instanceof Buffer, 'Expected stderr to be a Buffer');
  assert.equal(str + os.EOL, stdout.toString());
}));
