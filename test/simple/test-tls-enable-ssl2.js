'use strict';

var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var stdout = '';
var stderr = '';
var proc = spawn(process.execPath, ['--enable-ssl2', '-e', '0']);
proc.stdout.setEncoding('utf8');
proc.stderr.setEncoding('utf8');
proc.stdout.on('data', function(data) { stdout += data; });
proc.stderr.on('data', function(data) { stderr += data; });
proc.on('exit', common.mustCall(function(exitCode, signalCode) {
  assert.strictEqual(exitCode, 12);
  assert.strictEqual(signalCode, null);
}));
process.on('exit', function() {
  assert.strictEqual(stdout, '');
  assert(/Error: --enable-ssl2 is no longer supported/.test(stderr));
});
