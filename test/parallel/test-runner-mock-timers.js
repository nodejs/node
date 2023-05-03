'use strict';
process.env.NODE_TEST_KNOWN_GLOBALS = 0;
const common = require('../common');

const assert = require('node:assert');
const { it, mock, describe } = require('node:test');
const nodeTimers = require('node:timers');
const nodeTimersPromises = require('node:timers/promises');

describe('Mock Timers Test Suite', () => {
  describe('globals/timers', () => {
    describe('setTimeout Suite', () => {

      it('should advance in time and trigger timers when calling the .tick function', (t) => {
        mock.timers.enable();

        const fn = mock.fn();

        global.setTimeout(fn, 4000);

        mock.timers.tick(4000);
        assert.strictEqual(fn.mock.callCount(), 1);
        mock.timers.reset();
      });

      it('should advance in time and trigger timers when calling the .tick function multiple times', (t) => {
        t.mock.timers.enable();
        const fn = t.mock.fn();

        global.setTimeout(fn, 2000);

        t.mock.timers.tick(1000);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);

        assert.strictEqual(fn.mock.callCount(), 1);
        t.mock.timers.reset();
      });

      it('should keep setTimeout working if timers are disabled', (t, done) => {
        const now = Date.now();
        const timeout = 2;
        const expected = () => now - timeout;
        global.setTimeout(common.mustCall(() => {
          assert.strictEqual(now - timeout, expected());
          done();
        }), timeout);
      });

    });

    describe('clearTimeout Suite', () => {
      it('should not advance in time if clearTimeout was invoked', (t) => {
        t.mock.timers.enable()

        const fn = mock.fn();

        const id = global.setTimeout(fn, 4000);
        global.clearTimeout(id)
        t.mock.timers.tick(4000);

        assert.strictEqual(fn.mock.callCount(), 0);
        t.mock.timers.reset()
      });
    });

    describe('setInterval Suite', () => {
      it('should tick three times using fake setInterval', (t) => {
        t.mock.timers.enable();
        const fn = t.mock.fn();

        const id = global.setInterval(fn, 200);

        t.mock.timers.tick(200);
        t.mock.timers.tick(200);
        t.mock.timers.tick(200);

        global.clearInterval(id);

        assert.strictEqual(fn.mock.callCount(), 3);
        t.mock.timers.reset();
      });
    });

    describe('clearInterval Suite', () => {
      it('should not advance in time if clearInterval was invoked', (t) => {
        t.mock.timers.enable()

        const fn = mock.fn();
        const id = global.setInterval(fn, 200);
        global.clearInterval(id)
        t.mock.timers.tick(200);

        assert.strictEqual(fn.mock.callCount(), 0);
        t.mock.timers.reset()
      });
    });

  });

  describe('timers Suite', () => {
    describe('setTimeout Suite', () => {
      it('should advance in time and trigger timers when calling the .tick function multiple times', (t) => {
        t.mock.timers.enable();
        const fn = t.mock.fn();
        const { setTimeout } = nodeTimers;
        setTimeout(fn, 2000);

        t.mock.timers.tick(1000);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);

        assert.strictEqual(fn.mock.callCount(), 1);
      });
    });

    describe('clearTimeout Suite', () => {
      it('should not advance in time if clearTimeout was invoked', (t) => {
        t.mock.timers.enable()

        const fn = mock.fn();
        const { setTimeout, clearTimeout } = nodeTimers;
        const id = setTimeout(fn, 2000);
        clearTimeout(id)
        t.mock.timers.tick(2000);

        assert.strictEqual(fn.mock.callCount(), 0);
        t.mock.timers.reset()
      });
    });

    describe('setInterval Suite', () => {
      it('should tick three times using fake setInterval', (t) => {
        t.mock.timers.enable();
        const fn = t.mock.fn();

        const id = nodeTimers.setInterval(fn, 200);

        t.mock.timers.tick(200);
        t.mock.timers.tick(200);
        t.mock.timers.tick(200);
        t.mock.timers.tick(200);

        nodeTimers.clearInterval(id);

        assert.strictEqual(fn.mock.callCount(), 4);
        t.mock.timers.reset();
      });
    });

    describe('clearInterval Suite', () => {
      it('should not advance in time if clearInterval was invoked', (t) => {
        t.mock.timers.enable()

        const fn = mock.fn();
        const { setInterval, clearInterval } = nodeTimers;
        const id = setInterval(fn, 200);
        clearInterval(id)
        t.mock.timers.tick(200);

        assert.strictEqual(fn.mock.callCount(), 0);
        t.mock.timers.reset()
      });
    });
  });

  describe('timers/promises', () => {
    describe('setTimeout Suite', () => {
      it('should advance in time and trigger timers when calling the .tick function multiple times', async (t) => {
        t.mock.timers.enable();

        const p = nodeTimersPromises.setTimeout(2000);

        t.mock.timers.tick(1000);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);

        p.then(common.mustCall((result) => {
          assert.ok(result);
          t.mock.timers.reset();
        }));
      });
    });
  });
});
