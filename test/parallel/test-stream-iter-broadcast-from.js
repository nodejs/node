// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { broadcast, Broadcast, from, text } = require('stream/iter');
const { setImmediate } = require('timers/promises');

// =============================================================================
// Broadcast.from
// =============================================================================

async function testBroadcastFromAsyncIterable() {
  const source = from('broadcast-from');
  const { broadcast: bc } = Broadcast.from(source);
  const consumer = bc.push();

  const data = await text(consumer);
  assert.strictEqual(data, 'broadcast-from');
}

async function testBroadcastFromNonArrayChunks() {
  // Source that yields single Uint8Array chunks (not arrays)
  const enc = new TextEncoder();
  async function* singleChunkSource() {
    yield enc.encode('hello');
    yield enc.encode(' world');
  }
  const { broadcast: bc } = Broadcast.from(singleChunkSource());
  const consumer = bc.push();
  const data = await text(consumer);
  assert.strictEqual(data, 'hello world');
}

async function testBroadcastFromStringChunks() {
  // Source that yields bare strings (not arrays)
  async function* stringSource() {
    yield 'foo';
    yield 'bar';
  }
  const { broadcast: bc } = Broadcast.from(stringSource());
  const consumer = bc.push();
  const data = await text(consumer);
  assert.strictEqual(data, 'foobar');
}

async function testBroadcastFromStringInput() {
  const { broadcast: bc } = Broadcast.from('abc');
  const consumer = bc.push();
  const data = await text(consumer);
  assert.strictEqual(data, 'abc');
}

async function testBroadcastFromUint8ArrayInput() {
  const { broadcast: bc } = Broadcast.from(new Uint8Array([97]));
  const consumer = bc.push();
  const data = await text(consumer);
  assert.strictEqual(data, 'a');
}

async function testBroadcastFromDataViewInput() {
  const view = new DataView(new Uint8Array([104, 105]).buffer);
  const { broadcast: bc } = Broadcast.from(view);
  const consumer = bc.push();
  const data = await text(consumer);
  assert.strictEqual(data, 'hi');
}

async function testBroadcastFromMultipleConsumers() {
  const source = from('shared-data');
  const { broadcast: bc } = Broadcast.from(source);

  const c1 = bc.push();
  const c2 = bc.push();

  const [data1, data2] = await Promise.all([
    text(c1),
    text(c2),
  ]);

  assert.strictEqual(data1, 'shared-data');
  assert.strictEqual(data2, 'shared-data');
}

// =============================================================================
// AbortSignal
// =============================================================================

async function testAbortSignal() {
  const ac = new AbortController();
  const { broadcast: bc } = broadcast({ signal: ac.signal });
  const consumer = bc.push();

  ac.abort();

  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of consumer) {
      assert.fail('Should not reach here');
    }
  }, { name: 'AbortError' });
}

async function testAlreadyAbortedSignal() {
  const { broadcast: bc } = broadcast({ signal: AbortSignal.abort() });
  const consumer = bc.push();

  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of consumer) {
      assert.fail('Should not reach here');
    }
  }, { name: 'AbortError' });
}

// =============================================================================
// Broadcast.from() hang fix - cancel while write blocked on backpressure
// =============================================================================

async function testBroadcastFromCancelWhileBlocked() {
  let resolveNext;
  let sourceReturned = false;
  const source = {
    [Symbol.asyncIterator]() {
      return {
        next() {
          const { promise, resolve } = Promise.withResolvers();
          resolveNext = resolve;
          return promise;
        },
        return() {
          sourceReturned = true;
          return Promise.resolve({ __proto__: null, done: true });
        },
      };
    },
  };

  const { writer, broadcast: bc } = Broadcast.from(source);
  const iter = bc.push()[Symbol.asyncIterator]();
  const pendingRead = iter.next();
  await setImmediate();

  let writesAfterCancel = 0;
  writer.writevSync = () => { writesAfterCancel++; return true; };
  bc.cancel();
  assert.deepStrictEqual(await pendingRead, {
    __proto__: null,
    done: true,
    value: undefined,
  });

  resolveNext({
    __proto__: null,
    done: false,
    value: [new TextEncoder().encode('late')],
  });
  await setImmediate();
  assert.strictEqual(writesAfterCancel, 0);
  assert.strictEqual(sourceReturned, true);
}

// =============================================================================
// Source error propagation via Broadcast.from()
// =============================================================================

async function testBroadcastFromSourceError() {
  async function* failingSource() {
    yield [new TextEncoder().encode('a')];
    throw new Error('broadcast source boom');
  }
  const { broadcast: bc } = Broadcast.from(failingSource());
  const consumer = bc.push();
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of consumer) { /* consume */ }
  }, { message: 'broadcast source boom' });
}

// =============================================================================
// Protocol validation
// =============================================================================

function testBroadcastProtocolReturnsNull() {
  const obj = {
    [Symbol.for('Stream.broadcastProtocol')]() { return null; },
  };
  assert.throws(
    () => Broadcast.from(obj),
    { code: 'ERR_INVALID_RETURN_VALUE' },
  );
}

function testBroadcastProtocolReturnsString() {
  const obj = {
    [Symbol.for('Stream.broadcastProtocol')]() { return 'bad'; },
  };
  assert.throws(
    () => Broadcast.from(obj),
    { code: 'ERR_INVALID_RETURN_VALUE' },
  );
}

function testBroadcastProtocolReturnsUndefined() {
  const obj = {
    [Symbol.for('Stream.broadcastProtocol')]() { },
  };
  assert.throws(
    () => Broadcast.from(obj),
    { code: 'ERR_INVALID_RETURN_VALUE' },
  );
}

Promise.all([
  testBroadcastFromAsyncIterable(),
  testBroadcastFromNonArrayChunks(),
  testBroadcastFromStringChunks(),
  testBroadcastFromStringInput(),
  testBroadcastFromUint8ArrayInput(),
  testBroadcastFromDataViewInput(),
  testBroadcastFromMultipleConsumers(),
  testAbortSignal(),
  testAlreadyAbortedSignal(),
  testBroadcastFromCancelWhileBlocked(),
  testBroadcastFromSourceError(),
  testBroadcastProtocolReturnsNull(),
  testBroadcastProtocolReturnsString(),
  testBroadcastProtocolReturnsUndefined(),
]).then(common.mustCall());
