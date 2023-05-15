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

      it('should work with the same params as the original setTimeout', (t) => {
        t.mock.timers.enable();
        const fn = t.mock.fn();
        const args = ['a', 'b', 'c'];
        global.setTimeout(fn, 2000, ...args);

        t.mock.timers.tick(1000);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);

        assert.strictEqual(fn.mock.callCount(), 1);
        assert.deepStrictEqual(fn.mock.calls[0].arguments, args);
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
        t.mock.timers.enable();

        const fn = mock.fn();

        const id = global.setTimeout(fn, 4000);
        global.clearTimeout(id);
        t.mock.timers.tick(4000);

        assert.strictEqual(fn.mock.callCount(), 0);
        t.mock.timers.reset();
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

      it('should work with the same params as the original setInterval', (t) => {
        t.mock.timers.enable();
        const fn = t.mock.fn();
        const args = ['a', 'b', 'c'];
        const id = global.setInterval(fn, 200, ...args);

        t.mock.timers.tick(200);
        t.mock.timers.tick(200);
        t.mock.timers.tick(200);

        global.clearInterval(id);

        assert.strictEqual(fn.mock.callCount(), 3);
        assert.deepStrictEqual(fn.mock.calls[0].arguments, args);
        assert.deepStrictEqual(fn.mock.calls[1].arguments, args);
        assert.deepStrictEqual(fn.mock.calls[2].arguments, args);

        t.mock.timers.reset();
      });
    });

    describe('clearInterval Suite', () => {
      it('should not advance in time if clearInterval was invoked', (t) => {
        t.mock.timers.enable();

        const fn = mock.fn();
        const id = global.setInterval(fn, 200);
        global.clearInterval(id);
        t.mock.timers.tick(200);

        assert.strictEqual(fn.mock.callCount(), 0);
        t.mock.timers.reset();
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

      it('should work with the same params as the original timers.setTimeout', (t) => {
        t.mock.timers.enable();
        const fn = t.mock.fn();
        const { setTimeout } = nodeTimers;
        const args = ['a', 'b', 'c'];
        setTimeout(fn, 2000, ...args);

        t.mock.timers.tick(1000);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);

        assert.strictEqual(fn.mock.callCount(), 1);
        assert.deepStrictEqual(fn.mock.calls[0].arguments, args);
      });
    });

    describe('clearTimeout Suite', () => {
      it('should not advance in time if clearTimeout was invoked', (t) => {
        t.mock.timers.enable();

        const fn = mock.fn();
        const { setTimeout, clearTimeout } = nodeTimers;
        const id = setTimeout(fn, 2000);
        clearTimeout(id);
        t.mock.timers.tick(2000);

        assert.strictEqual(fn.mock.callCount(), 0);
        t.mock.timers.reset();
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

      it('should work with the same params as the original timers.setInterval', (t) => {
        t.mock.timers.enable();
        const fn = t.mock.fn();
        const args = ['a', 'b', 'c'];
        const id = nodeTimers.setInterval(fn, 200, ...args);

        t.mock.timers.tick(200);
        t.mock.timers.tick(200);
        t.mock.timers.tick(200);
        t.mock.timers.tick(200);

        nodeTimers.clearInterval(id);

        assert.strictEqual(fn.mock.callCount(), 4);
        assert.deepStrictEqual(fn.mock.calls[0].arguments, args);
        assert.deepStrictEqual(fn.mock.calls[1].arguments, args);
        assert.deepStrictEqual(fn.mock.calls[2].arguments, args);
        assert.deepStrictEqual(fn.mock.calls[3].arguments, args);

        t.mock.timers.reset();
      });
    });

    describe('clearInterval Suite', () => {
      it('should not advance in time if clearInterval was invoked', (t) => {
        t.mock.timers.enable();

        const fn = mock.fn();
        const { setInterval, clearInterval } = nodeTimers;
        const id = setInterval(fn, 200);
        clearInterval(id);
        t.mock.timers.tick(200);

        assert.strictEqual(fn.mock.callCount(), 0);
        t.mock.timers.reset();
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

      it('should work with the same params as the original timers/promises/setTimeout', async (t) => {
        t.mock.timers.enable();
        const expectedResult = 'result';
        const controller = new AbortController();
        const p = nodeTimersPromises.setTimeout(2000, expectedResult, {
          ref: true,
          signal: controller.signal
        });

        t.mock.timers.tick(1000);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);

        const result = await p;
        assert.strictEqual(result, expectedResult);
        t.mock.timers.reset();
      });

      it('should abort operation if timers/promises/setTimeout received an aborted signal', async (t) => {
        t.mock.timers.enable();
        const expectedResult = 'result';
        const controller = new AbortController();
        const p = nodeTimersPromises.setTimeout(2000, expectedResult, {
          ref: true,
          signal: controller.signal
        });

        t.mock.timers.tick(1000);
        controller.abort();
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);
        await assert.rejects(() => p, {
          name: 'AbortError',
        });

        t.mock.timers.reset();
      });
      it('should abort operation even if the .tick wasn\'t called', async (t) => {
        t.mock.timers.enable();
        const expectedResult = 'result';
        const controller = new AbortController();
        const p = nodeTimersPromises.setTimeout(2000, expectedResult, {
          ref: true,
          signal: controller.signal
        });

        controller.abort();

        await assert.rejects(() => p, {
          name: 'AbortError',
        });

        t.mock.timers.reset();
      });

      it('should reject given an an invalid signal instance', async (t) => {
        t.mock.timers.enable();
        const expectedResult = 'result';
        const p = nodeTimersPromises.setTimeout(2000, expectedResult, {
          ref: true,
          signal: {}
        });

        await assert.rejects(() => p, {
          name: 'TypeError',
          code: 'ERR_INVALID_ARG_TYPE'
        });

        t.mock.timers.reset();
      });
    });

    describe('setInterval Suite', () => {
      it('should tick three times using fake setInterval', async (t) => {
        t.mock.timers.enable();

        const interval = 100;
        const intervalIterator = nodeTimersPromises.setInterval(interval, Date.now());

        const first = intervalIterator.next();
        const second = intervalIterator.next();
        const third = intervalIterator.next();

        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);

        const results = await Promise.all([
          first,
          second,
          third,
        ]);

        const finished = await intervalIterator.return();
        assert.deepStrictEqual(finished, { done: true, value: undefined });

        results.forEach((result) => {
          assert.strictEqual(typeof result.value, 'number');
          assert.strictEqual(result.done, false);
        });

        t.mock.timers.reset();
      });

      it('should tick five times testing a real use case', async (t) => {

        t.mock.timers.enable();

        const expectedIterations = 5;
        const interval = 1000;

        async function run() {
          const timers = [];
          for await (const startTime of nodeTimersPromises.setInterval(interval, Date.now())) {
            timers.push(startTime);
            if (timers.length === expectedIterations) break;

          }
          return timers;
        }

        const r = run();
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);

        const timersResults = await r;
        assert.strictEqual(timersResults.length, expectedIterations);
        t.mock.timers.reset();
      });
    });

  });
});
