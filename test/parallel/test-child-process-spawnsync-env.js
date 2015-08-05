'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

if (process.argv[2] === 'child') {
  console.log(process.env.foo);
} else {
  var expected = 'bar';
  var child = cp.spawnSync(process.execPath, [__filename, 'child'], {
    env: {foo: expected}
  });

  assert.equal(child.stdout.toString().trim(), expected);
}
