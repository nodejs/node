'use strict';
const { ObjectAssign, ObjectDefineProperty } = primordials;
const { test, describe, it, before, after, beforeEach, afterEach } = require('internal/test_runner/harness');
const { run } = require('internal/test_runner/runner');

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

let lazyMock;

ObjectDefineProperty(module.exports, 'mock', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get() {
    if (lazyMock === undefined) {
      const { MockTracker } = require('internal/test_runner/mock');

      lazyMock = new MockTracker();
    }

    return lazyMock;
  },
});

let lazyFakeTimers;

ObjectDefineProperty(module.exports, 'fakeTimers', {
  __proto__: null,
  configurable: true,
  enumerable: true,
  get() {
    if (lazyFakeTimers === undefined) {
      const { FakeTimers } = require('internal/test_runner/fake_timers');

      lazyFakeTimers = new FakeTimers();
    }

    return lazyFakeTimers;
  },
});
