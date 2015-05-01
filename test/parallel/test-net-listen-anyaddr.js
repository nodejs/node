var assert = require('assert');
var common = require('../common');
var cp = require('child_process');

var args = [
  '-e',
  "require('net').createServer().listen(0, function() { this.close(); });",
];

// should exit cleanly almost immediately
var opts = {
  timeout: common.platformTimeout(1000),
};

cp.execFile(process.execPath, args, opts, assert.ifError);
