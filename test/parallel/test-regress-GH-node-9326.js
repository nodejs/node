'use strict';
require('../common');
var assert = require('assert');
var child_process = require('child_process');

// NOTE: Was crashing on FreeBSD
var cp = child_process.spawn(process.execPath, [
  '-e',
  'process.kill(process.pid, "SIGINT")'
]);

cp.on('exit', function(code) {
  assert.notEqual(code, 0);
});
