'use strict';
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;
const os = require('os');
const str = 'hello';

// default encoding
exec('echo ' + str, common.mustCall(function(err, stdout, stderr) {
  assert.strictEqual(typeof stdout, 'string', 'Expected stdout to be a string');
  assert.strictEqual(typeof stderr, 'string', 'Expected stderr to be a string');
  assert.strictEqual(str + os.EOL, stdout);
}));

// no encoding (Buffers expected)
exec('echo ' + str, {
  encoding: null
}, common.mustCall(function(err, stdout, stderr) {
  assert.strictEqual(stdout instanceof Buffer, true,
                     'Expected stdout to be a Buffer');
  assert.strictEqual(stderr instanceof Buffer, true,
                     'Expected stderr to be a Buffer');
  assert.strictEqual(str + os.EOL, stdout.toString());
}));
