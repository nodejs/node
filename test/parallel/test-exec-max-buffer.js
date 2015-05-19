'use strict';
var common = require('../common');
var exec = require('child_process').exec;
var assert = require('assert');

var cmd = 'echo "hello world"';

exec(cmd, { maxBuffer: 5 }, function(err, stdout, stderr) {
  assert.ok(err);
  assert.ok(/maxBuffer/.test(err.message));
});
