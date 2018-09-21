'use strict';
const common = require('../common');
const assert = require('assert');
const os = require('os');
const {
  PRIORITY_LOW,
  PRIORITY_BELOW_NORMAL,
  PRIORITY_NORMAL,
  PRIORITY_ABOVE_NORMAL,
  PRIORITY_HIGH,
  PRIORITY_HIGHEST
} = os.constants.priority;

// Validate priority constants.
assert.strictEqual(typeof PRIORITY_LOW, 'number');
assert.strictEqual(typeof PRIORITY_BELOW_NORMAL, 'number');
assert.strictEqual(typeof PRIORITY_NORMAL, 'number');
assert.strictEqual(typeof PRIORITY_ABOVE_NORMAL, 'number');
assert.strictEqual(typeof PRIORITY_HIGH, 'number');
assert.strictEqual(typeof PRIORITY_HIGHEST, 'number');

// Test pid type validation.
[null, true, false, 'foo', {}, [], /x/].forEach((pid) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "pid" argument must be of type number\./
  };

  common.expectsError(() => {
    os.setPriority(pid, PRIORITY_NORMAL);
  }, errObj);

  common.expectsError(() => {
    os.getPriority(pid);
  }, errObj);
});

// Test pid range validation.
[NaN, Infinity, -Infinity, 3.14, 2 ** 32].forEach((pid) => {
  const errObj = {
    code: 'ERR_OUT_OF_RANGE',
    message: /The value of "pid" is out of range\./
  };

  common.expectsError(() => {
    os.setPriority(pid, PRIORITY_NORMAL);
  }, errObj);

  common.expectsError(() => {
    os.getPriority(pid);
  }, errObj);
});

// Test priority type validation.
[null, true, false, 'foo', {}, [], /x/].forEach((priority) => {
  common.expectsError(() => {
    os.setPriority(0, priority);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    message: /The "priority" argument must be of type number\./
  });
});

// Test priority range validation.
[
  NaN,
  Infinity,
  -Infinity,
  3.14,
  2 ** 32,
  PRIORITY_HIGHEST - 1,
  PRIORITY_LOW + 1
].forEach((priority) => {
  common.expectsError(() => {
    os.setPriority(0, priority);
  }, {
    code: 'ERR_OUT_OF_RANGE',
    message: /The value of "priority" is out of range\./
  });
});

// Verify that valid values work.
for (let i = PRIORITY_HIGHEST; i <= PRIORITY_LOW; i++) {
  // A pid of 0 corresponds to the current process.
  try {
    os.setPriority(0, i);
  } catch (err) {
    // The current user might not have sufficient permissions to set this
    // specific priority level. Skip this priority, but keep trying lower
    // priorities.
    if (err.info.code === 'EACCES')
      continue;

    assert(err);
  }

  checkPriority(0, i);

  // An undefined pid corresponds to the current process.
  os.setPriority(i);
  checkPriority(undefined, i);

  // Specifying the actual pid works.
  os.setPriority(process.pid, i);
  checkPriority(process.pid, i);
}


function checkPriority(pid, expected) {
  const priority = os.getPriority(pid);

  // Verify that the priority values match on Unix, and are range mapped on
  // Windows.
  if (!common.isWindows) {
    assert.strictEqual(priority, expected);
    return;
  }

  // On Windows setting PRIORITY_HIGHEST will only work for elevated user,
  // for others it will be silently reduced to PRIORITY_HIGH
  if (expected < PRIORITY_HIGH)
    assert.ok(priority === PRIORITY_HIGHEST || priority === PRIORITY_HIGH);
  else if (expected < PRIORITY_ABOVE_NORMAL)
    assert.strictEqual(priority, PRIORITY_HIGH);
  else if (expected < PRIORITY_NORMAL)
    assert.strictEqual(priority, PRIORITY_ABOVE_NORMAL);
  else if (expected < PRIORITY_BELOW_NORMAL)
    assert.strictEqual(priority, PRIORITY_NORMAL);
  else if (expected < PRIORITY_LOW)
    assert.strictEqual(priority, PRIORITY_BELOW_NORMAL);
  else
    assert.strictEqual(priority, PRIORITY_LOW);
}
