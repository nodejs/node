'use strict';
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;
let success_count = 0;
let error_count = 0;
let response = '';
let child;

function after(err, stdout, stderr) {
  if (err) {
    error_count++;
    console.log('error!: ' + err.code);
    console.log('stdout: ' + JSON.stringify(stdout));
    console.log('stderr: ' + JSON.stringify(stderr));
    assert.strictEqual(false, err.killed);
  } else {
    success_count++;
    assert.notStrictEqual('', stdout);
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
  assert.strictEqual(1, success_count);
  assert.strictEqual(0, error_count);
  assert.ok(response.indexOf('HELLO=WORLD') >= 0);
});
