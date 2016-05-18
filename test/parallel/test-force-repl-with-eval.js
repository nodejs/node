'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

// spawn a node child process in "interactive" mode (force the repl) and eval
const cp = spawn(process.execPath, ['-i', '-e', 'console.log("42")']);
var gotToEnd = false;
const timeoutId = setTimeout(function() {
  throw new Error('timeout!');
}, common.platformTimeout(1000)); // give node + the repl 1 second to boot up

cp.stdout.setEncoding('utf8');

var output = '';
cp.stdout.on('data', function(b) {
  output += b;
  if (output === '> 42\n') {
    clearTimeout(timeoutId);
    gotToEnd = true;
    cp.kill();
  }
});

process.on('exit', function() {
  assert(gotToEnd);
});
