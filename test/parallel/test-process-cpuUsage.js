'use strict';
require('../common');
const assert = require('assert');

const result = process.cpuUsage();

// Validate the result of calling with no previous value argument.
validateResult(result);

// Validate the result of calling with a previous value argument.
validateResult(process.cpuUsage(result));

// Ensure the results are >= the previous.
let thisUsage;
let lastUsage = process.cpuUsage();
for (let i = 0; i < 10; i++) {
  thisUsage = process.cpuUsage();
  validateResult(thisUsage);
  assert(thisUsage.user >= lastUsage.user);
  assert(thisUsage.system >= lastUsage.system);
  lastUsage = thisUsage;
}

// Ensure that the diffs are >= 0.
let startUsage;
let diffUsage;
for (let i = 0; i < 10; i++) {
  startUsage = process.cpuUsage();
  diffUsage = process.cpuUsage(startUsage);
  validateResult(startUsage);
  validateResult(diffUsage);
  assert(diffUsage.user >= 0);
  assert(diffUsage.system >= 0);
}

// Ensure that an invalid shape for the previous value argument throws an error.
assert.throws(function() { process.cpuUsage(1); });
assert.throws(function() { process.cpuUsage({}); });
assert.throws(function() { process.cpuUsage({ user: 'a' }); });
assert.throws(function() { process.cpuUsage({ system: 'b' }); });
assert.throws(function() { process.cpuUsage({ user: null, system: 'c' }); });
assert.throws(function() { process.cpuUsage({ user: 'd', system: null }); });
assert.throws(function() { process.cpuUsage({ user: -1, system: 2 }); });
assert.throws(function() { process.cpuUsage({ user: 3, system: -2 }); });
assert.throws(function() {
  process.cpuUsage({
    user: Number.POSITIVE_INFINITY,
    system: 4
  });
});
assert.throws(function() {
  process.cpuUsage({
    user: 5,
    system: Number.NEGATIVE_INFINITY
  });
});

// Ensure that the return value is the expected shape.
function validateResult(result) {
  assert.notEqual(result, null);

  assert(Number.isFinite(result.user));
  assert(Number.isFinite(result.system));

  assert(result.user >= 0);
  assert(result.system >= 0);
}
