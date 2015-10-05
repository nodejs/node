'use strict';
var common = require('../common');
var assert = require('assert');
var cp = require('child_process');

if (process.argv[2] === 'child') {
  console.log(process.env.foo);
} else {
  var expected = 'bar';
  var opt = {
    env: process.env
  };
  opt.env.foo = expected;

  var child = cp.spawnSync(process.execPath, [__filename, 'child'], opt);

  assert.equal(child.stdout.toString().trim(), expected);
}
