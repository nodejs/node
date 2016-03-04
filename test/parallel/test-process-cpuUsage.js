'use strict';
require('../common');
var assert = require('assert');

// the default behavior, return { user: [int, int], system: [int, int]}
var result = process.cpuUsage();

// validate the default behavior
validateResult(result);

// validate that passing an existing result returns another valid result
validateResult(process.cpuUsage(result));

// test that only a previous result object may be passed to process.cpuUsage()
assert.throws(function() { process.cpuUsage(1); });
assert.throws(function() { process.cpuUsage({}); });
assert.throws(function() { process.cpuUsage({ user: [0, 0] }); });
assert.throws(function() { process.cpuUsage({ system: [0, 0] }); });
assert.throws(function() { process.cpuUsage({ user: null, system: [0, 0] }); });
assert.throws(function() { process.cpuUsage({ user: [0, 0], system: null }); });

function validateResult(result) {
  assert(result.user != null);
  assert(result.system != null);

  assert(Array.isArray(result.user));
  assert(Array.isArray(result.system));

  assert.equal(2, result.user.length);
  assert.equal(2, result.system.length);

  result.user.forEach(function(v) {
    assert.equal('number', typeof v);
    assert(isFinite(v));
  });

  result.system.forEach(function(v) {
    assert.equal('number', typeof v);
    assert(isFinite(v));
  });
}

// ensure we don't end up sending negative values on second rollovers
// https://github.com/nodejs/node/issues/4751
const diff = process.cpuUsage({ user: [0, 1e9 - 1], system: [0, 1e9 - 1] });
assert(diff.user[1] >= 0);
assert(diff.system[1] >= 0);
