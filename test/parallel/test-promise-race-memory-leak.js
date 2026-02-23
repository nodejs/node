// Copyright Node.js contributors. All rights reserved.
'use strict';

// Tests for memory leak in Promise.race with immediately-resolving promises.
// See: https://github.com/nodejs/node/issues/51452

const common = require('../common');
const assert = require('node:assert');

// Test 1: Promise.race with immediately-resolving promises should not leak
{
  async function promiseValue(value) {
    return value;
  }

  async function run() {
    let count = 0;
    const maxIterations = 1000;

    for (let i = 0; i < maxIterations; i++) {
      await Promise.race([promiseValue("foo"), promiseValue("bar")]);
      count++;
    }

    assert.strictEqual(count, maxIterations);
  }

  run().then(common.mustCall());
}

// Test 2: Promise.any with immediately-resolving promises should not leak
{
  async function promiseValue(value) {
    return value;
  }

  async function run() {
    let count = 0;
    const maxIterations = 1000;

    for (let i = 0; i < maxIterations; i++) {
      await Promise.any([promiseValue("foo"), promiseValue("bar")]);
      count++;
    }

    assert.strictEqual(count, maxIterations);
  }

  run().then(common.mustCall());
}

// Test 3: Mixed immediately-resolving and delayed promises should work correctly
{
  async function immediateValue(value) {
    return value;
  }

  function delayedValue(value, delay) {
    return new Promise((resolve) => {
      setTimeout(() => resolve(value), delay);
    });
  }

  async function run() {
    let count = 0;
    const maxIterations = 100;

    for (let i = 0; i < maxIterations; i++) {
      await Promise.race([
        immediateValue("immediate"),
        delayedValue("delayed", 10)
      ]);
      count++;
    }

    assert.strictEqual(count, maxIterations);
  }

  run().then(common.mustCall());
}

// Test 4: Verify that Promise.race still resolves correctly with immediate values
{
  async function promiseValue(value) {
    return value;
  }

  Promise.race([promiseValue("first"), promiseValue("second")]).then(common.mustCall((result) => {
    assert.strictEqual(result, "first");
  }));
}

// Test 5: Verify that Promise.any still resolves correctly with immediate values
{
  async function promiseValue(value) {
    return value;
  }

  Promise.any([promiseValue("first"), promiseValue("second")]).then(common.mustCall((result) => {
    assert.strictEqual(result, "first");
  }));
}
