'use strict';
const common = require('../common');
var assert = require('assert');
var exec = require('child_process').exec;

if (common.isOSX) {
  common.skip('Output of `id -G` is unreliable on Darwin.');
  return;
}

if (typeof process.getgroups === 'function') {
  var groups = process.getgroups();
  assert(Array.isArray(groups));
  assert(groups.length > 0);
  exec('id -G', function(err, stdout) {
    if (err) throw err;
    var real_groups = stdout.match(/\d+/g).map(Number);
    assert.equal(groups.length, real_groups.length);
    check(groups, real_groups);
    check(real_groups, groups);
  });
}

function check(a, b) {
  for (var i = 0; i < a.length; ++i) assert.notStrictEqual(b.indexOf(a[i]), -1);
}
