'use strict';
const common = require('../common');
const assert = require('assert');
const ch = require('child_process');

var SIZE = 100000;
var childGone = false;

var cp = ch.spawn('python', ['-c', 'print ' + SIZE + ' * "C"'], {
  stdio: 'inherit'
});

cp.on('exit', function(code) {
  childGone = true;
  assert.equal(0, code);
});

process.on('exit', function() {
  assert.ok(childGone);
});
