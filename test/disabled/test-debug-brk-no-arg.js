'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var child = spawn(process.execPath, ['--debug-brk=' + common.PORT]);
child.stderr.once('data', function(c) {
  console.error('%j', c.toString());
  child.stdin.end();
});

child.on('exit', function(c) {
  assert(c === 0);
  console.log('ok');
});
