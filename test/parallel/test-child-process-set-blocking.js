'use strict';
const common = require('../common');
var assert = require('assert');
var ch = require('child_process');

var SIZE = 100000;

var cp = ch.spawn('python', ['-c', 'print ' + SIZE + ' * "C"'], {
  stdio: 'inherit'
});

cp.on('exit', common.mustCall(function(code) {
  assert.equal(0, code);
}));
