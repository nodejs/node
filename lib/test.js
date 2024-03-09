'use strict';
const { ObjectAssign, ObjectDefineProperty } = primordials;
const { test, describe, it, before, after, beforeEach, afterEach, beforeEachSuite, afterEachSuite } = require('internal/test_runner/harness');
const { run } = require('internal/test_runner/runner');

module.exports = test;
ObjectAssign(module.exports, {
  after,
  afterEach,
  afterEachSuite,
  before,
  beforeEach,
  beforeEachSuite,
  describe,
  it,
  run,
  test,
});

let lazyMock;

ObjectDefineProperty(module.exports, 'mock', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get() {
    if (lazyMock === undefined) {
      const { MockTracker } = require('internal/test_runner/mock/mock');

      lazyMock = new MockTracker();
    }

    return lazyMock;
  },
});
