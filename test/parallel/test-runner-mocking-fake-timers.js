'use strict';
const common = require('../common');
process.env.NODE_TEST_KNOWN_GLOBALS = 0;

const assert = require('node:assert');
const { it, mock, afterEach, describe } = require('node:test');
const { fakeTimers } = mock;

describe('Faketimers Test Suite', () => {

  describe('setTimeout Suite', () => {
    afterEach(() => fakeTimers.reset());

    it('should advance in time and trigger timers when calling the .tick function', (t) => {
      fakeTimers.enable();

      const fn = mock.fn(() => {});

      global.setTimeout(fn, 4000);

      fakeTimers.tick(4000);
      assert.ok(fn.mock.callCount());
    });

    it('should advance in time and trigger timers when calling the .tick function multiple times', (t) => {
      fakeTimers.enable();
      const fn = mock.fn();

      global.setTimeout(fn, 2000);

      fakeTimers.tick(1000);
      fakeTimers.tick(1000);

      assert.strictEqual(fn.mock.callCount(), 1);
    });

    it('should keep setTimeout working if fakeTimers are disabled', (t, done) => {
      const now = Date.now();
      const timeout = 2;
      const expected = () => now - timeout;
      global.setTimeout(common.mustCall(() => {
        assert.strictEqual(now - timeout, expected());
        done();
      }), timeout);
    });

  });
});
