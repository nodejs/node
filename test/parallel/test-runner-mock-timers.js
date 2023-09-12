'use strict';
process.env.NODE_TEST_KNOWN_GLOBALS = 0;
const common = require('../common');

const assert = require('node:assert');
const { it, mock, describe } = require('node:test');
const nodeTimers = require('node:timers');
const nodeTimersPromises = require('node:timers/promises');

describe('Mock Timers Test Suite', () => {
  describe('MockTimers API', () => {
    it('should throw an error if trying to enable a timer that is not supported', (t) => {
      assert.throws(() => {
        t.mock.timers.enable(['DOES_NOT_EXIST']);
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
      });
    });

    it('should throw an error if trying to enable a timer twice', (t) => {
      t.mock.timers.enable();
      assert.throws(() => {
        t.mock.timers.enable();
      }, {
        code: 'ERR_INVALID_STATE',
      });
    });

    it('should not throw if calling reset without enabling timers', (t) => {
      t.mock.timers.reset();
    });

    it('should throw an error if calling tick without enabling timers', (t) => {
      assert.throws(() => {
        t.mock.timers.tick();
      }, {
        code: 'ERR_INVALID_STATE',
      });
    });

    it('should throw an error if calling tick with a negative number', (t) => {
      t.mock.timers.enable();
      assert.throws(() => {
        t.mock.timers.tick(-1);
      }, {
        code: 'ERR_INVALID_ARG_VALUE',
      });
    });
    it('should check that propertyDescriptor gets back after resetting timers', (t) => {
      const getDescriptor = (ctx, fn) => Object.getOwnPropertyDescriptor(ctx, fn);
      const getCurrentTimersDescriptors = () => {
        const timers = [
          'setTimeout',
          'clearTimeout',
          'setInterval',
          'clearInterval',
          'setImmediate',
          'clearImmediate',
        ];

        const globalTimersDescriptors = timers.map((fn) => getDescriptor(global, fn));
        const nodeTimersDescriptors = timers.map((fn) => getDescriptor(nodeTimers, fn));
        const nodeTimersPromisesDescriptors = timers
          .filter((fn) => !fn.includes('clear'))
          .map((fn) => getDescriptor(nodeTimersPromises, fn));

        return {
          global: globalTimersDescriptors,
          nodeTimers: nodeTimersDescriptors,
          nodeTimersPromises: nodeTimersPromisesDescriptors,
        };
      };

      const originalDescriptors = getCurrentTimersDescriptors();

      t.mock.timers.enable();
      const during = getCurrentTimersDescriptors();
      t.mock.timers.reset();
      const after = getCurrentTimersDescriptors();

      for (const env in originalDescriptors) {
        for (const prop in originalDescriptors[env]) {
          const originalDescriptor = originalDescriptors[env][prop];
          const afterDescriptor = after[env][prop];

          assert.deepStrictEqual(
            originalDescriptor,
            afterDescriptor,
          );

          assert.notDeepStrictEqual(
            originalDescriptor,
            during[env][prop],
          );

          assert.notDeepStrictEqual(
            during[env][prop],
            after[env][prop],
          );

        }
      }
    });

    it('should reset all timers when calling .reset function', (t) => {
      t.mock.timers.enable();
      const fn = t.mock.fn();
      global.setTimeout(fn, 1000);
      t.mock.timers.reset();
      assert.throws(() => {
        t.mock.timers.tick(1000);
      }, {
        code: 'ERR_INVALID_STATE',
      });

      assert.strictEqual(fn.mock.callCount(), 0);
    });

    it('should reset all timers when calling Symbol.dispose', (t) => {
      t.mock.timers.enable();
      const fn = t.mock.fn();
      global.setTimeout(fn, 1000);
      // TODO(benjamingr) refactor to `using`
      t.mock.timers[Symbol.dispose]();
      assert.throws(() => {
        t.mock.timers.tick(1000);
      }, {
        code: 'ERR_INVALID_STATE',
      });

      assert.strictEqual(fn.mock.callCount(), 0);
    });

    it('should execute in order if timeout is the same', (t) => {
      t.mock.timers.enable();
      const order = [];
      const fn1 = t.mock.fn(() => order.push('f1'));
      const fn2 = t.mock.fn(() => order.push('f2'));
      global.setTimeout(fn1, 1000);
      global.setTimeout(fn2, 1000);
      t.mock.timers.tick(1000);
      assert.strictEqual(fn1.mock.callCount(), 1);
      assert.strictEqual(fn2.mock.callCount(), 1);
      assert.deepStrictEqual(order, ['f1', 'f2']);
    });

    describe('runAll Suite', () => {
      it('should throw an error if calling runAll without enabling timers', (t) => {
        assert.throws(() => {
          t.mock.timers.runAll();
        }, {
          code: 'ERR_INVALID_STATE',
        });
      });

      it('should trigger all timers when calling .runAll function', async (t) => {
        const timeoutFn = t.mock.fn();
        const intervalFn = t.mock.fn();

        t.mock.timers.enable();
        global.setTimeout(timeoutFn, 1111);
        const id = global.setInterval(intervalFn, 9999);
        t.mock.timers.runAll();

        global.clearInterval(id);
        assert.strictEqual(timeoutFn.mock.callCount(), 1);
        assert.strictEqual(intervalFn.mock.callCount(), 1);
      });
    });

  });

  describe('globals/timers', () => {
    describe('setTimeout Suite', () => {
      it('should advance in time and trigger timers when calling the .tick function', (t) => {
        mock.timers.enable(['setTimeout']);

        const fn = mock.fn();

        global.setTimeout(fn, 4000);

        mock.timers.tick(4000);
        assert.strictEqual(fn.mock.callCount(), 1);
        mock.timers.reset();
      });

      it('should advance in time and trigger timers when calling the .tick function multiple times', (t) => {
        t.mock.timers.enable(['setTimeout']);
        const fn = t.mock.fn();

        global.setTimeout(fn, 2000);

        t.mock.timers.tick(1000);
        assert.strictEqual(fn.mock.callCount(), 0);
        t.mock.timers.tick(500);
        assert.strictEqual(fn.mock.callCount(), 0);
        t.mock.timers.tick(500);
        assert.strictEqual(fn.mock.callCount(), 1);
      });

      it('should work with the same params as the original setTimeout', (t) => {
        t.mock.timers.enable(['setTimeout']);
        const fn = t.mock.fn();
        const args = ['a', 'b', 'c'];
        global.setTimeout(fn, 2000, ...args);

        t.mock.timers.tick(1000);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);

        assert.strictEqual(fn.mock.callCount(), 1);
        assert.deepStrictEqual(fn.mock.calls[0].arguments, args);
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
        t.mock.timers.enable(['setTimeout']);

        const fn = mock.fn();

        const id = global.setTimeout(fn, 4000);
        global.clearTimeout(id);
        t.mock.timers.tick(4000);

        assert.strictEqual(fn.mock.callCount(), 0);
      });
    });

    describe('setInterval Suite', () => {
      it('should tick three times using fake setInterval', (t) => {
        t.mock.timers.enable(['setInterval']);
        const fn = t.mock.fn();

        const id = global.setInterval(fn, 200);

        t.mock.timers.tick(200);
        t.mock.timers.tick(200);
        t.mock.timers.tick(200);

        global.clearInterval(id);

        assert.strictEqual(fn.mock.callCount(), 3);
      });

      it('should work with the same params as the original setInterval', (t) => {
        t.mock.timers.enable(['setInterval']);
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

      });
    });

    describe('clearInterval Suite', () => {
      it('should not advance in time if clearInterval was invoked', (t) => {
        t.mock.timers.enable(['setInterval']);

        const fn = mock.fn();
        const id = global.setInterval(fn, 200);
        global.clearInterval(id);
        t.mock.timers.tick(200);

        assert.strictEqual(fn.mock.callCount(), 0);
      });
    });

    describe('setImmediate Suite', () => {
      it('should keep setImmediate working if timers are disabled', (t, done) => {
        const now = Date.now();
        const timeout = 2;
        const expected = () => now - timeout;
        global.setImmediate(common.mustCall(() => {
          assert.strictEqual(now - timeout, expected());
          done();
        }));
      });

      it('should work with the same params as the original setImmediate', (t) => {
        t.mock.timers.enable(['setImmediate']);
        const fn = t.mock.fn();
        const args = ['a', 'b', 'c'];
        global.setImmediate(fn, ...args);
        t.mock.timers.tick(9999);

        assert.strictEqual(fn.mock.callCount(), 1);
        assert.deepStrictEqual(fn.mock.calls[0].arguments, args);
      });

      it('should not advance in time if clearImmediate was invoked', (t) => {
        t.mock.timers.enable(['setImmediate']);

        const id = global.setImmediate(common.mustNotCall());
        global.clearImmediate(id);
        t.mock.timers.tick(200);
      });

      it('should advance in time and trigger timers when calling the .tick function', (t) => {
        t.mock.timers.enable(['setImmediate']);
        global.setImmediate(common.mustCall(1));
        t.mock.timers.tick(0);
      });

      it('should execute in order if setImmediate is called multiple times', (t) => {
        t.mock.timers.enable(['setImmediate']);
        const order = [];
        const fn1 = t.mock.fn(common.mustCall(() => order.push('f1'), 1));
        const fn2 = t.mock.fn(common.mustCall(() => order.push('f2'), 1));

        global.setImmediate(fn1);
        global.setImmediate(fn2);

        t.mock.timers.tick(0);

        assert.deepStrictEqual(order, ['f1', 'f2']);
      });

      it('should execute setImmediate first if setTimeout was also called', (t) => {
        t.mock.timers.enable(['setImmediate', 'setTimeout']);
        const order = [];
        const fn1 = t.mock.fn(common.mustCall(() => order.push('f1'), 1));
        const fn2 = t.mock.fn(common.mustCall(() => order.push('f2'), 1));

        global.setTimeout(fn2, 0);
        global.setImmediate(fn1);

        t.mock.timers.tick(100);

        assert.deepStrictEqual(order, ['f1', 'f2']);
      });
    });
  });

  describe('timers Suite', () => {
    describe('setTimeout Suite', () => {
      it('should advance in time and trigger timers when calling the .tick function multiple times', (t) => {
        t.mock.timers.enable(['setTimeout']);
        const fn = t.mock.fn();
        const { setTimeout } = nodeTimers;
        setTimeout(fn, 2000);

        t.mock.timers.tick(1000);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);

        assert.strictEqual(fn.mock.callCount(), 1);
      });

      it('should work with the same params as the original timers.setTimeout', (t) => {
        t.mock.timers.enable(['setTimeout']);
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
        t.mock.timers.enable(['setTimeout']);

        const fn = mock.fn();
        const { setTimeout, clearTimeout } = nodeTimers;
        const id = setTimeout(fn, 2000);
        clearTimeout(id);
        t.mock.timers.tick(2000);

        assert.strictEqual(fn.mock.callCount(), 0);
      });
    });

    describe('setInterval Suite', () => {
      it('should tick three times using fake setInterval', (t) => {
        t.mock.timers.enable(['setInterval']);
        const fn = t.mock.fn();

        const id = nodeTimers.setInterval(fn, 200);

        t.mock.timers.tick(200);
        t.mock.timers.tick(200);
        t.mock.timers.tick(200);
        t.mock.timers.tick(200);

        nodeTimers.clearInterval(id);

        assert.strictEqual(fn.mock.callCount(), 4);
      });

      it('should work with the same params as the original timers.setInterval', (t) => {
        t.mock.timers.enable(['setInterval']);
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

      });
    });

    describe('clearInterval Suite', () => {
      it('should not advance in time if clearInterval was invoked', (t) => {
        t.mock.timers.enable(['setInterval']);

        const fn = mock.fn();
        const { setInterval, clearInterval } = nodeTimers;
        const id = setInterval(fn, 200);
        clearInterval(id);
        t.mock.timers.tick(200);

        assert.strictEqual(fn.mock.callCount(), 0);
      });
    });

    describe('setImmediate Suite', () => {
      it('should keep setImmediate working if timers are disabled', (t, done) => {
        const now = Date.now();
        const timeout = 2;
        const expected = () => now - timeout;
        nodeTimers.setImmediate(common.mustCall(() => {
          assert.strictEqual(now - timeout, expected());
          done();
        }, 1));
      });

      it('should work with the same params as the original setImmediate', (t) => {
        t.mock.timers.enable(['setImmediate']);
        const fn = t.mock.fn();
        const args = ['a', 'b', 'c'];
        nodeTimers.setImmediate(fn, ...args);
        t.mock.timers.tick(9999);

        assert.strictEqual(fn.mock.callCount(), 1);
        assert.deepStrictEqual(fn.mock.calls[0].arguments, args);
      });

      it('should not advance in time if clearImmediate was invoked', (t) => {
        t.mock.timers.enable(['setImmediate']);

        const id = nodeTimers.setImmediate(common.mustNotCall());
        nodeTimers.clearImmediate(id);
        t.mock.timers.tick(200);
      });

      it('should advance in time and trigger timers when calling the .tick function', (t) => {
        t.mock.timers.enable(['setImmediate']);
        nodeTimers.setImmediate(common.mustCall(1));
        t.mock.timers.tick(0);
      });

      it('should execute in order if setImmediate is called multiple times', (t) => {
        t.mock.timers.enable(['setImmediate']);
        const order = [];
        const fn1 = t.mock.fn(common.mustCall(() => order.push('f1'), 1));
        const fn2 = t.mock.fn(common.mustCall(() => order.push('f2'), 1));

        nodeTimers.setImmediate(fn1);
        nodeTimers.setImmediate(fn2);

        t.mock.timers.tick(0);

        assert.deepStrictEqual(order, ['f1', 'f2']);
      });

      it('should execute setImmediate first if setTimeout was also called', (t) => {
        t.mock.timers.enable(['setImmediate', 'setTimeout']);
        const order = [];
        const fn1 = t.mock.fn(common.mustCall(() => order.push('f1'), 1));
        const fn2 = t.mock.fn(common.mustCall(() => order.push('f2'), 1));

        nodeTimers.setTimeout(fn2, 0);
        nodeTimers.setImmediate(fn1);

        t.mock.timers.tick(100);

        assert.deepStrictEqual(order, ['f1', 'f2']);
      });
    });
  });

  describe('timers/promises', () => {
    describe('setTimeout Suite', () => {
      it('should advance in time and trigger timers when calling the .tick function multiple times', async (t) => {
        t.mock.timers.enable(['setTimeout']);

        const p = nodeTimersPromises.setTimeout(2000);

        t.mock.timers.tick(1000);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);
        t.mock.timers.tick(500);

        p.then(common.mustCall((result) => {
          assert.strictEqual(result, undefined);
        }));
      });

      it('should work with the same params as the original timers/promises/setTimeout', async (t) => {
        t.mock.timers.enable(['setTimeout']);
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
      });

      it('should abort operation if timers/promises/setTimeout received an aborted signal', async (t) => {
        t.mock.timers.enable(['setTimeout']);
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

      });
      it('should abort operation even if the .tick wasn\'t called', async (t) => {
        t.mock.timers.enable(['setTimeout']);
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

      });

      it('should abort operation when .abort is called before calling setTimeout', async (t) => {
        t.mock.timers.enable(['setTimeout']);
        const expectedResult = 'result';
        const controller = new AbortController();
        controller.abort();
        const p = nodeTimersPromises.setTimeout(2000, expectedResult, {
          ref: true,
          signal: controller.signal
        });

        await assert.rejects(() => p, {
          name: 'AbortError',
        });

      });

      it('should reject given an an invalid signal instance', async (t) => {
        t.mock.timers.enable(['setTimeout']);
        const expectedResult = 'result';
        const p = nodeTimersPromises.setTimeout(2000, expectedResult, {
          ref: true,
          signal: {}
        });

        await assert.rejects(() => p, {
          name: 'TypeError',
          code: 'ERR_INVALID_ARG_TYPE'
        });

      });
    });

    describe('setInterval Suite', () => {
      it('should tick three times using fake setInterval', async (t) => {
        t.mock.timers.enable(['setInterval']);

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

      });
      it('should tick five times testing a real use case', async (t) => {

        t.mock.timers.enable(['setInterval']);

        const expectedIterations = 5;
        const interval = 1000;
        const startedAt = Date.now();
        async function run() {
          const times = [];
          for await (const time of nodeTimersPromises.setInterval(interval, startedAt)) {
            times.push(time);
            if (times.length === expectedIterations) break;

          }
          return times;
        }

        const r = run();
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);

        const timeResults = await r;
        assert.strictEqual(timeResults.length, expectedIterations);
        for (let it = 1; it < expectedIterations; it++) {
          assert.strictEqual(timeResults[it - 1], startedAt + (interval * it));
        }
      });

      it('should abort operation given an abort controller signal', async (t) => {
        t.mock.timers.enable(['setInterval']);

        const interval = 100;
        const abortController = new AbortController();
        const intervalIterator = nodeTimersPromises.setInterval(interval, Date.now(), {
          signal: abortController.signal
        });

        const first = intervalIterator.next();
        const second = intervalIterator.next();

        t.mock.timers.tick(interval);
        abortController.abort();
        t.mock.timers.tick(interval);

        const firstResult = await first;
        // Interval * 2 because value can be a little bit greater than interval
        assert.ok(firstResult.value < Date.now() + interval * 2);
        assert.strictEqual(firstResult.done, false);

        await assert.rejects(() => second, {
          name: 'AbortError',
        });

      });

      it('should abort operation when .abort is called before calling setInterval', async (t) => {
        t.mock.timers.enable(['setInterval']);

        const interval = 100;
        const abortController = new AbortController();
        abortController.abort();
        const intervalIterator = nodeTimersPromises.setInterval(interval, Date.now(), {
          signal: abortController.signal
        });

        const first = intervalIterator.next();
        t.mock.timers.tick(interval);

        await assert.rejects(() => first, {
          name: 'AbortError',
        });
      });

      it('should abort operation given an abort controller signal on a real use case', async (t) => {
        t.mock.timers.enable(['setInterval']);
        const controller = new AbortController();
        const signal = controller.signal;
        const interval = 200;
        const expectedIterations = 2;
        const startedAt = Date.now();
        const timeResults = [];
        async function run() {
          const it = nodeTimersPromises.setInterval(interval, startedAt, { signal });
          for await (const time of it) {
            timeResults.push(time);
            if (timeResults.length === 5) break;
          }
        }

        const r = run();
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);
        controller.abort();
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);
        t.mock.timers.tick(interval);

        await assert.rejects(() => r, {
          name: 'AbortError',
        });
        assert.strictEqual(timeResults.length, expectedIterations);

        for (let it = 1; it < expectedIterations; it++) {
          assert.strictEqual(timeResults[it - 1], startedAt + (interval * it));
        }

      });

    });

    describe('setImmediate Suite', () => {
      it('should advance in time and trigger timers when calling the .tick function multiple times', (t, done) => {
        t.mock.timers.enable(['setImmediate']);
        const p = nodeTimersPromises.setImmediate();

        t.mock.timers.tick(5555);

        p.then(common.mustCall((result) => {
          assert.strictEqual(result, undefined);
          done();
        }, 1));
      });

      it('should work with the same params as the original timers/promises/setImmediate', async (t) => {
        t.mock.timers.enable(['setImmediate']);
        const expectedResult = 'result';
        const controller = new AbortController();
        const p = nodeTimersPromises.setImmediate(expectedResult, {
          ref: true,
          signal: controller.signal
        });

        t.mock.timers.tick(500);

        const result = await p;
        assert.strictEqual(result, expectedResult);
      });

      it('should abort operation if timers/promises/setImmediate received an aborted signal', async (t) => {
        t.mock.timers.enable(['setImmediate']);
        const expectedResult = 'result';
        const controller = new AbortController();
        const p = nodeTimersPromises.setImmediate(expectedResult, {
          ref: true,
          signal: controller.signal
        });

        controller.abort();
        t.mock.timers.tick(0);

        await assert.rejects(() => p, {
          name: 'AbortError',
        });

      });
      it('should abort operation even if the .tick wasn\'t called', async (t) => {
        t.mock.timers.enable(['setImmediate']);
        const expectedResult = 'result';
        const controller = new AbortController();
        const p = nodeTimersPromises.setImmediate(expectedResult, {
          ref: true,
          signal: controller.signal
        });

        controller.abort();

        await assert.rejects(() => p, {
          name: 'AbortError',
        });
      });

      it('should abort operation when .abort is called before calling setImmediate', async (t) => {
        t.mock.timers.enable(['setImmediate']);
        const expectedResult = 'result';
        const controller = new AbortController();
        controller.abort();
        const p = nodeTimersPromises.setImmediate(expectedResult, {
          ref: true,
          signal: controller.signal
        });

        await assert.rejects(() => p, {
          name: 'AbortError',
        });

      });

      it('should reject given an an invalid signal instance', async (t) => {
        t.mock.timers.enable(['setImmediate']);
        const expectedResult = 'result';
        const p = nodeTimersPromises.setImmediate(expectedResult, {
          ref: true,
          signal: {}
        });

        await assert.rejects(() => p, {
          name: 'TypeError',
          code: 'ERR_INVALID_ARG_TYPE'
        });

      });

      it('should execute in order if setImmediate is called multiple times', async (t) => {
        t.mock.timers.enable(['setImmediate']);

        const p1 = nodeTimersPromises.setImmediate('fn1');
        const p2 = nodeTimersPromises.setImmediate('fn2');

        t.mock.timers.tick(0);

        const results = await Promise.race([p1, p2]);

        assert.strictEqual(results, 'fn1');
      });
    });
  });
});
