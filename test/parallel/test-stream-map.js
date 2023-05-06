'use strict';

const common = require('../common');
const {
  Readable,
} = require('stream');
const assert = require('assert');
const { once } = require('events');
const { setTimeout } = require('timers/promises');
const { it } = require('node:test');

it('Map works on synchronous streams with a synchronous mapper', async () => {
  const stream = Readable.from([1, 2, 3, 4, 5]).map((x) => x + x);

  assert.deepStrictEqual(await stream.toArray(), [2, 4, 6, 8, 10]);
});

it('Map works on synchronous streams with an asynchronous mapper', async () => {
  const stream = Readable.from([1, 2, 3, 4, 5]).map(async (x) => {
    await Promise.resolve();
    return x + x;
  });

  assert.deepStrictEqual(await stream.toArray(), [2, 4, 6, 8, 10]);
});

it('Map works on asynchronous streams with a asynchronous mapper', async () => {
  const stream = Readable.from([1, 2, 3, 4, 5]).map(async (x) => {
    return x + x;
  }).map((x) => x + x);

  assert.deepStrictEqual(await stream.toArray(), [4, 8, 12, 16, 20]);
});

it('Map works on an infinite stream', async () => {
  const stream = Readable.from(async function* () {
    while (true) yield 1;
  }()).map(common.mustCall(async (x) => {
    return x + x;
  }, 5));

  let i = 1;
  for await (const item of stream) {
    assert.strictEqual(item, 2);
    if (++i === 5) break;
  }
});

it('Map works on non-objectMode streams', async () => {
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

  for await (const item of stream) {
    assert.strictEqual(item, result.shift());
  }
});

it('Does not care about data events', async () => {
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

  for await (const item of stream) {
    assert.strictEqual(item, result.shift());
  }
});


it('Emitting an error during `map`', async () => {
  // Emitting an error during `map`
  const stream = Readable.from([1, 2, 3, 4, 5]).map(async (x) => {
    if (x === 3) {
      stream.emit('error', new Error('boom'));
    }
    return x + x;
  });
  await assert.rejects(
    stream.map((x) => x + x).toArray(),
    /boom/,
  );
});

it('Throwing an error during `map` (sync)', async () => {
  const stream = Readable.from([1, 2, 3, 4, 5]).map((x) => {
    if (x === 3) {
      throw new Error('boom');
    }
    return x + x;
  });

  await assert.rejects(
    stream.map((x) => x + x).toArray(),
    /boom/,
  );
});

it('Throwing an error during `map` (async)', async () => {
  const stream = Readable.from([1, 2, 3, 4, 5]).map(async (x) => {
    if (x === 3) {
      throw new Error('boom');
    }
    return x + x;
  });

  await assert.rejects(
    stream.map((x) => x + x).toArray(),
    /boom/,
  );
});

it('Concurrency + AbortSignal', async () => {
  const ac = new AbortController();
  const range = Readable.from([1, 2, 3, 4, 5]);
  const stream = range.map(common.mustCall(async (_, { signal }) => {
    await once(signal, 'abort');
    throw signal.reason;
  }, 2), { signal: ac.signal, concurrency: 2 });

  // pump
  const pr = assert.rejects(async () => {
    for await (const item of stream) {
      assert.fail('should not reach here, got ' + item);
    }
  }, {
    name: 'AbortError',
  });

  setImmediate(() => {
    ac.abort();
  });

  await pr;
});

it('Concurrency result order', async () => {
  const stream = Readable.from([1, 2]).map(async (item, { signal }) => {
    await setTimeout(10 - item, { signal });
    return item;
  }, { concurrency: 2 });

  const expected = [1, 2];
  for await (const item of stream) {
    assert.strictEqual(item, expected.shift());
  }
});

it('Error cases', async () => {
  await assert.throws(() => Readable.from([1]).map(1), /ERR_INVALID_ARG_TYPE/);
  await assert.throws(() => Readable.from([1]).map((x) => x, {
    concurrency: 'Foo'
  }), /ERR_OUT_OF_RANGE/);
  await assert.throws(() => Readable.from([1]).map((x) => x, 1), /ERR_INVALID_ARG_TYPE/);
  await assert.throws(() => Readable.from([1]).map((x) => x, { signal: true }), /ERR_INVALID_ARG_TYPE/);
});

it('Test result is a Readable', () => {
  const stream = Readable.from([1, 2, 3, 4, 5]).map((x) => x);
  assert.strictEqual(stream.readable, true);
});
