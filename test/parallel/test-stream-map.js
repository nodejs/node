'use strict';

const common = require('../common');
const {
  Readable,
} = require('stream');
const assert = require('assert');
const { once } = require('events');
const { setTimeout } = require('timers/promises');

function createDependentPromises(n) {
  const promiseAndResolveArray = [];

  for (let i = 0; i < n; i++) {
    let res;
    const promise = new Promise((resolve) => {
      if (i === 0) {
        res = resolve;
        return;
      }
      res = () => promiseAndResolveArray[i - 1][0].then(resolve);
    });

    promiseAndResolveArray.push([promise, res]);
  }

  return promiseAndResolveArray;
}

{
  // Map works on synchronous streams with a synchronous mapper
  const stream = Readable.from([1, 2, 3, 4, 5]).map((x) => x + x);
  (async () => {
    assert.deepStrictEqual(await stream.toArray(), [2, 4, 6, 8, 10]);
  })().then(common.mustCall());
}

{
  // Map works on synchronous streams with an asynchronous mapper
  const stream = Readable.from([1, 2, 3, 4, 5]).map(async (x) => {
    await Promise.resolve();
    return x + x;
  });
  (async () => {
    assert.deepStrictEqual(await stream.toArray(), [2, 4, 6, 8, 10]);
  })().then(common.mustCall());
}

{
  // Map works on asynchronous streams with a asynchronous mapper
  const stream = Readable.from([1, 2, 3, 4, 5]).map(async (x) => {
    return x + x;
  }).map((x) => x + x);
  (async () => {
    assert.deepStrictEqual(await stream.toArray(), [4, 8, 12, 16, 20]);
  })().then(common.mustCall());
}

{
  // Map works on an infinite stream
  const stream = Readable.from(async function* () {
    while (true) yield 1;
  }()).map(common.mustCall(async (x) => {
    return x + x;
  }, 5));
  (async () => {
    let i = 1;
    for await (const item of stream) {
      assert.strictEqual(item, 2);
      if (++i === 5) break;
    }
  })().then(common.mustCall());
}

{
  // Map works on non-objectMode streams
  const stream = new Readable({
    read() {
      this.push(Uint8Array.from([1]));
      this.push(Uint8Array.from([2]));
      this.push(null);
    }
  }).map(async ([x]) => {
    return x + x;
  }).map((x) => x + x);
  const result = [4, 8];
  (async () => {
    for await (const item of stream) {
      assert.strictEqual(item, result.shift());
    }
  })().then(common.mustCall());
}

{
  // Does not care about data events
  const source = new Readable({
    read() {
      this.push(Uint8Array.from([1]));
      this.push(Uint8Array.from([2]));
      this.push(null);
    }
  });
  setImmediate(() => stream.emit('data', Uint8Array.from([1])));
  const stream = source.map(async ([x]) => {
    return x + x;
  }).map((x) => x + x);
  const result = [4, 8];
  (async () => {
    for await (const item of stream) {
      assert.strictEqual(item, result.shift());
    }
  })().then(common.mustCall());
}

{
  // Emitting an error during `map`
  const stream = Readable.from([1, 2, 3, 4, 5]).map(async (x) => {
    if (x === 3) {
      stream.emit('error', new Error('boom'));
    }
    return x + x;
  });
  assert.rejects(
    stream.map((x) => x + x).toArray(),
    /boom/,
  ).then(common.mustCall());
}

{
  // Throwing an error during `map` (sync)
  const stream = Readable.from([1, 2, 3, 4, 5]).map((x) => {
    if (x === 3) {
      throw new Error('boom');
    }
    return x + x;
  });
  assert.rejects(
    stream.map((x) => x + x).toArray(),
    /boom/,
  ).then(common.mustCall());
}


{
  // Throwing an error during `map` (async)
  const stream = Readable.from([1, 2, 3, 4, 5]).map(async (x) => {
    if (x === 3) {
      throw new Error('boom');
    }
    return x + x;
  });
  assert.rejects(
    stream.map((x) => x + x).toArray(),
    /boom/,
  ).then(common.mustCall());
}

{
  // Concurrency + AbortSignal
  const ac = new AbortController();
  const range = Readable.from([1, 2, 3, 4, 5]);
  const stream = range.map(common.mustCall(async (_, { signal }) => {
    await once(signal, 'abort');
    throw signal.reason;
  }, 2), { signal: ac.signal, concurrency: 2, highWaterMark: 0 });
  // pump
  assert.rejects(async () => {
    for await (const item of stream) {
      assert.fail('should not reach here, got ' + item);
    }
  }, {
    name: 'AbortError',
  }).then(common.mustCall());

  setImmediate(() => {
    ac.abort();
  });
}

{
  // Concurrency result order
  const stream = Readable.from([1, 2]).map(async (item, { signal }) => {
    await setTimeout(10 - item, { signal });
    return item;
  }, { concurrency: 2 });

  (async () => {
    const expected = [1, 2];
    for await (const item of stream) {
      assert.strictEqual(item, expected.shift());
    }
  })().then(common.mustCall());
}


{
  // highWaterMark with small concurrency
  const finishOrder = [];

  const promises = createDependentPromises(4);

  const raw = Readable.from([2, 0, 1, 3]);
  const stream = raw.map(async (item) => {
    const [promise, resolve] = promises[item];
    resolve();

    await promise;
    finishOrder.push(item);
    return item;
  }, { concurrency: 2 });

  (async () => {
    await stream.toArray();

    assert.deepStrictEqual(finishOrder, [0, 1, 2, 3]);
  })().then(common.mustCall(), common.mustNotCall());
}

{
  // highWaterMark with a lot of items and large concurrency
  const finishOrder = [];

  const promises = createDependentPromises(20);

  const input = [10, 1, 0, 3, 4, 2, 5, 7, 8, 9, 6, 11, 12, 13, 18, 15, 16, 17, 14, 19];
  const raw = Readable.from(input);
  // Should be
  // 10, 1, 0, 3, 4, 2 | next: 0
  // 10, 1, 3, 4, 2, 5 | next: 1
  // 10, 3, 4, 2, 5, 7 | next: 2
  // 10, 3, 4, 5, 7, 8 | next: 3
  // 10, 4, 5, 7, 8, 9 | next: 4
  // 10, 5, 7, 8, 9, 6 | next: 5
  // 10, 7, 8, 9, 6, 11 | next: 6
  // 10, 7, 8, 9, 11, 12 | next: 7
  // 10, 8, 9, 11, 12, 13 | next: 8
  // 10, 9, 11, 12, 13, 18 | next: 9
  // 10, 11, 12, 13, 18, 15 | next: 10
  // 11, 12, 13, 18, 15, 16 | next: 11
  // 12, 13, 18, 15, 16, 17 | next: 12
  // 13, 18, 15, 16, 17, 14 | next: 13
  // 18, 15, 16, 17, 14, 19 | next: 14
  // 18, 15, 16, 17, 19 | next: 15
  // 18, 16, 17, 19 | next: 16
  // 18, 17, 19 | next: 17
  // 18, 19 | next: 18
  // 19 | next: 19
  //

  const stream = raw.map(async (item) => {
    const [promise, resolve] = promises[item];
    resolve();

    await promise;
    finishOrder.push(item);
    return item;
  }, { concurrency: 6 });

  (async () => {
    const outputOrder = await stream.toArray();

    assert.deepStrictEqual(outputOrder, input);
    assert.deepStrictEqual(finishOrder, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19]);
  })().then(common.mustCall(), common.mustNotCall());
}

{
  // Custom highWaterMark with a lot of items and large concurrency
  const finishOrder = [];

  const promises = createDependentPromises(20);

  const input = [11, 1, 0, 3, 4, 2, 5, 7, 8, 9, 6, 10, 12, 13, 18, 15, 16, 17, 14, 19];
  const raw = Readable.from(input);
  // Should be
  // 11, 1, 0, 3, 4     | next: 0, buffer: []
  // 11, 1, 3, 4, 2     | next: 1, buffer: [0]
  // 11, 3, 4, 2, 5     | next: 2, buffer: [0, 1]
  // 11, 3, 4, 5, 7     | next: 3, buffer: [0, 1, 2]
  // 11, 4, 5, 7, 8     | next: 4, buffer: [0, 1, 2, 3]
  // 11, 5, 7, 8, 9     | next: 5, buffer: [0, 1, 2, 3, 4]
  // 11, 7, 8, 9, 6     | next: 6, buffer: [0, 1, 2, 3, 4, 5]
  // 11, 7, 8, 9, 10    | next: 7, buffer: [0, 1, 2, 3, 4, 5, 6] -- buffer full
  // 11, 8, 9, 10, 12   | next: 8, buffer: [0, 1, 2, 3, 4, 5, 6]
  // 11, 9, 10, 12, 13  | next: 9, buffer: [0, 1, 2, 3, 4, 5, 6]
  // 11, 10, 12, 13, 18 | next: 10, buffer: [0, 1, 2, 3, 4, 5, 6]
  // 11, 12, 13, 18, 15 | next: 11, buffer: [0, 1, 2, 3, 4, 5, 6]
  // 12, 13, 18, 15, 16 | next: 12, buffer: [] -- all items flushed as 11 is consumed and all the items wait for it
  // 13, 18, 15, 16, 17 | next: 13, buffer: []
  // 18, 15, 16, 17, 14 | next: 14, buffer: []
  // 18, 15, 16, 17, 19 | next: 15, buffer: [14]
  // 18, 16, 17, 19     | next: 16, buffer: [14, 15]
  // 18, 17, 19         | next: 17, buffer: [14, 15, 16]
  // 18, 19             | next: 18, buffer: [14, 15, 16, 17]
  // 19                 | next: 19, buffer: [] -- all items flushed
  //

  const stream = raw.map(async (item) => {
    const [promise, resolve] = promises[item];
    resolve();

    await promise;
    finishOrder.push(item);
    return item;
  }, { concurrency: 5, highWaterMark: 7 });

  (async () => {
    const outputOrder = await stream.toArray();

    assert.deepStrictEqual(outputOrder, input);
    assert.deepStrictEqual(finishOrder, [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19]);
  })().then(common.mustCall(), common.mustNotCall());
}

{
  // Where there is a delay between the first and the next item it should not wait for filled queue
  // before yielding to the user
  const promises = createDependentPromises(3);

  const raw = Readable.from([0, 1, 2]);

  const stream = raw
      .map(async (item) => {
        if (item !== 0) {
          await promises[item][0];
        }

        return item;
      }, { concurrency: 2 })
      .map((item) => {
        // eslint-disable-next-line no-unused-vars
        for (const [_, resolve] of promises) {
          resolve();
        }

        return item;
      });

  (async () => {
    await stream.toArray();
  })().then(common.mustCall(), common.mustNotCall());
}

{
  // Error cases
  assert.throws(() => Readable.from([1]).map(1), /ERR_INVALID_ARG_TYPE/);
  assert.throws(() => Readable.from([1]).map((x) => x, {
    concurrency: 'Foo'
  }), /ERR_OUT_OF_RANGE/);
  assert.throws(() => Readable.from([1]).map((x) => x, {
    concurrency: -1
  }), /ERR_OUT_OF_RANGE/);
  assert.throws(() => Readable.from([1]).map((x) => x, 1), /ERR_INVALID_ARG_TYPE/);
  assert.throws(() => Readable.from([1]).map((x) => x, { signal: true }), /ERR_INVALID_ARG_TYPE/);
}
{
  // Test result is a Readable
  const stream = Readable.from([1, 2, 3, 4, 5]).map((x) => x);
  assert.strictEqual(stream.readable, true);
}
