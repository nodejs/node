'use strict';
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

if (common.isOSX) {
  common.skip('Output of `id -G` is unreliable on Darwin.');
  return;
}

if (typeof process.getgroups === 'function') {
  const groups = process.getgroups();
  assert(Array.isArray(groups));
  assert(groups.length > 0);
  exec('id -G', function(err, stdout) {
    if (err) throw err;
    const real_groups = stdout.match(/\d+/g).map(Number);
    assert.equal(groups.length, real_groups.length);
    check(groups, real_groups);
    check(real_groups, groups);
  });
}

function check(a, b) {
  for (let i = 0; i < a.length; ++i) assert.notStrictEqual(b.indexOf(a[i]), -1);
}
