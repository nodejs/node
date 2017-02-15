'use strict';
const common = require('../common');
const assert = require('assert');
const ch = require('child_process');

const SIZE = 100000;

const cp = ch.spawn('python', ['-c', 'print ' + SIZE + ' * "C"'], {
  stdio: 'inherit'
});

cp.on('exit', common.mustCall(function(code) {
  assert.strictEqual(0, code);
}));
