'use strict';
const common = require('../common');
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
assert.throws(
  () => process.cpuUsage(1),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "prevValue" argument must be of type object. ' +
             'Received type number (1)'
  }
);

// Check invalid types.
[
  {},
  { user: 'a' },
  { user: null, system: 'c' },
].forEach((value) => {
  assert.throws(
    () => process.cpuUsage(value),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "prevValue.user" property must be of type number.' +
               common.invalidArgTypeHelper(value.user)
    }
  );
});

[
  { user: 3, system: 'b' },
  { user: 3, system: null },
].forEach((value) => {
  assert.throws(
    () => process.cpuUsage(value),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError',
      message: 'The "prevValue.system" property must be of type number.' +
               common.invalidArgTypeHelper(value.system)
    }
  );
});

// Check invalid values.
[
  { user: -1, system: 2 },
  { user: Number.POSITIVE_INFINITY, system: 4 },
].forEach((value) => {
  assert.throws(
    () => process.cpuUsage(value),
    {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'RangeError',
      message: "The property 'prevValue.user' is invalid. " +
        `Received ${value.user}`,
    }
  );
});

[
  { user: 3, system: -2 },
  { user: 5, system: Number.NEGATIVE_INFINITY },
].forEach((value) => {
  assert.throws(
    () => process.cpuUsage(value),
    {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'RangeError',
      message: "The property 'prevValue.system' is invalid. " +
        `Received ${value.system}`,
    }
  );
});

// Ensure that the return value is the expected shape.
function validateResult(result) {
  assert.notStrictEqual(result, null);

  assert(Number.isFinite(result.user));
  assert(Number.isFinite(result.system));

  assert(result.user >= 0);
  assert(result.system >= 0);
}
