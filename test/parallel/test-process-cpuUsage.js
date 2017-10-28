'use strict';
const assert = require('assert');
const common = require('../common');
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
const invalidUserArgument = common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "preValue.user" property must be of type number'
}, 8);

const invalidSystemArgument = common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "preValue.system" property must be of type number'
}, 2);


// Ensure that an invalid shape for the previous value argument throws an error.
assert.throws(() => {
  process.cpuUsage(1);
}, invalidUserArgument);

assert.throws(() => {
  process.cpuUsage({});
}, invalidUserArgument);

assert.throws(() => {
  process.cpuUsage({ user: 'a' });
}, invalidUserArgument);

assert.throws(() => {
  process.cpuUsage({ system: 'b' });
}, invalidUserArgument);

assert.throws(() => {
  process.cpuUsage({ user: null, system: 'c' });
}, invalidUserArgument);

assert.throws(() => {
  process.cpuUsage({ user: 'd', system: null });
}, invalidUserArgument);

assert.throws(() => {
  process.cpuUsage({ user: -1, system: 2 });
}, invalidUserArgument);

assert.throws(() => {
  process.cpuUsage({ user: 3, system: -2 });
}, invalidSystemArgument);

assert.throws(() => {
  process.cpuUsage({
    user: Number.POSITIVE_INFINITY,
    system: 4
  });
}, invalidUserArgument);

assert.throws(() => {
  process.cpuUsage({
    user: 5,
    system: Number.NEGATIVE_INFINITY
  });
}, invalidSystemArgument);

// Ensure that the return value is the expected shape.
function validateResult(result) {
  assert.notStrictEqual(result, null);

  assert(Number.isFinite(result.user));
  assert(Number.isFinite(result.system));

  assert(result.user >= 0);
  assert(result.system >= 0);
}
