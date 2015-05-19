'use strict';
var common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  // Just reference stdin, it should start it
  process.stdin;
  return;
}

var proc = spawn(process.execPath, [__filename, 'child'], {
  stdio: ['ipc', 'inherit', 'inherit']
});

var childCode = -1;
proc.on('exit', function(code) {
  childCode = code;
});

process.on('exit', function() {
  assert.equal(childCode, 0);
});
