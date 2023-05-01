'use strict';
const common = require('../common');
process.env.NODE_TEST_KNOWN_GLOBALS = 0;

const assert = require('node:assert');
const { it, mock, afterEach, describe } = require('node:test');
const { fakeTimers } = mock;

describe('Faketimers Test Suite', () => {

  // describe('setInterval Suite', () => {
  //   it('should advance in time and trigger timers when calling the .tick function', (t) => {
  //     t.mock.fakeTimers.enable();

  //     const fn = mock.fn(() => {});

  //     const id = global.setInterval(fn, 200);

  //     t.mock.fakeTimers.tick(200);
  //     console.log('ae')
  //     t.mock.fakeTimers.tick(200);
  //     console.log('ae1')
  //     t.mock.fakeTimers.tick(200);
  //     console.log('ae3')
  //     t.mock.fakeTimers.tick(200);
  //     console.log('ae4')
  //     global.clearInterval(id)

  //     assert.strictEqual(fn.mock.callCount(), 4);
  //   });
  // });

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
      t.mock.fakeTimers.enable();
      const fn = t.mock.fn();

      global.setTimeout(fn, 2000);

      t.mock.fakeTimers.tick(1000);
      t.mock.fakeTimers.tick(500);
      t.mock.fakeTimers.tick(500);

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
