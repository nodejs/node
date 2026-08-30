// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { broadcast, push } = require('stream/iter');

const factories = [
  () => push({ budget: 16384 }),
  () => {
    const { writer, broadcast: bc } = broadcast({ budget: 16384 });
    return { __proto__: null, writer, readable: bc.push() };
  },
];

async function testWritevReentrancy() {
  for (const factory of factories) {
    const { writer, readable } = factory();
    const chunks = [];
    Object.defineProperty(chunks, 0, {
      __proto__: null,
      enumerable: true,
      get: common.mustCall(() => {
        assert.strictEqual(
          writer.writeSync(new Uint8Array(16384)), true);
        return Uint8Array.of(42);
      }),
    });

    let resolved = false;
    const write = writer.writev(chunks).then(common.mustCall(() => {
      resolved = true;
    }));
    await new Promise(setImmediate);
    assert.strictEqual(resolved, false);

    const iterator = readable[Symbol.asyncIterator]();
    assert.strictEqual((await iterator.next()).value[0].byteLength, 16384);
    await write;
    assert.strictEqual((await iterator.next()).value[0][0], 42);
    writer.endSync();
    assert.strictEqual((await iterator.next()).done, true);
  }

  for (const factory of factories) {
    const { writer, readable } = factory();
    const chunks = [];
    Object.defineProperty(chunks, 0, {
      __proto__: null,
      enumerable: true,
      get: common.mustCall(() => {
        writer.endSync();
        return Uint8Array.of(42);
      }),
    });

    await assert.rejects(writer.writev(chunks), {
      code: 'ERR_INVALID_STATE',
    });
    assert.strictEqual(
      (await readable[Symbol.asyncIterator]().next()).done, true);
  }
}

testWritevReentrancy().then(common.mustCall());
