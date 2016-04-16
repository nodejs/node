'use strict';
var common = require('../common');
var assert = require('assert');
var exec = require('child_process').exec;
var success_count = 0;
var error_count = 0;
var response = '';
var child;

function after(err, stdout, stderr) {
  if (err) {
    error_count++;
    console.log('error!: ' + err.code);
    console.log('stdout: ' + JSON.stringify(stdout));
    console.log('stderr: ' + JSON.stringify(stderr));
    assert.equal(false, err.killed);
  } else {
    success_count++;
    assert.equal(true, stdout != '');
  }
}

if (!common.isWindows) {
  child = exec('/usr/bin/env', { env: { 'HELLO': 'WORLD' } }, after);
} else {
  child = exec('set', { env: { 'HELLO': 'WORLD' } }, after);
}

child.stdout.setEncoding('utf8');
child.stdout.on('data', function(chunk) {
  response += chunk;
});

process.on('exit', function() {
  console.log('response: ', response);
  assert.equal(1, success_count);
  assert.equal(0, error_count);
  assert.ok(response.indexOf('HELLO=WORLD') >= 0);
});
