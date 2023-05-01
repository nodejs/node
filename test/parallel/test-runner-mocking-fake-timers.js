'use strict';
process.env.NODE_TEST_KNOWN_GLOBALS = 0;
const common = require('../common');

const assert = require('node:assert');
const { it, mock, afterEach, describe } = require('node:test');
const nodeTimers = require('node:timers');
const nodeTimersPromises = require('node:timers/promises');

const { fakeTimers } = mock;

describe('Faketimers Test Suite', () => {
  describe('globals/timers', () => {
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

  describe('timers Suite', () => {
    describe('setTimeout Suite', () => {
      it('should advance in time and trigger timers when calling the .tick function multiple times', (t) => {
        t.mock.fakeTimers.enable();
        const fn = t.mock.fn();
        const { setTimeout } = nodeTimers;
        setTimeout(fn, 2000);

        t.mock.fakeTimers.tick(1000);
        t.mock.fakeTimers.tick(500);
        t.mock.fakeTimers.tick(500);

        assert.strictEqual(fn.mock.callCount(), 1);
      });
    });
  });

  describe('timers/promises', () => {
    describe('setTimeout Suite', () => {
      it('should advance in time and trigger timers when calling the .tick function multiple times', async (t) => {
        t.mock.fakeTimers.enable();

        const p = nodeTimersPromises.setTimeout(2000);

        t.mock.fakeTimers.tick(1000);
        t.mock.fakeTimers.tick(500);
        t.mock.fakeTimers.tick(500);
        t.mock.fakeTimers.tick(500);

        p.finally(common.mustCall(() => assert.ok(p !== 0)));
      });
    });
  });
});
