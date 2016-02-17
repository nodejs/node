'use strict';
require('../common');
var assert = require('assert');
var cp = require('child_process');

if (process.argv[2] === 'child') {
  console.log(process.env.foo);
} else {
  var expected = 'bar';
  var child = cp.spawnSync(process.execPath, [__filename, 'child'], {
    env: Object.assign(process.env, { foo: expected })
  });

  assert.equal(child.stdout.toString().trim(), expected);
}
