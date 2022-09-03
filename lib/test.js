'use strict';
const { ObjectAssign } = primordials;
const { test, describe, it, before, after, beforeEach, afterEach } = require('internal/test_runner/harness');
const { run } = require('internal/test_runner/runner');
const { emitExperimentalWarning } = require('internal/util');

emitExperimentalWarning('The test runner');

module.exports = test;
ObjectAssign(module.exports, {
  after,
  afterEach,
  before,
  beforeEach,
  describe,
  it,
  run,
  test,
});
