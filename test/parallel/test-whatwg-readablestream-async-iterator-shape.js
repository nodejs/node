'use strict';

const common = require('../common');
const assert = require('assert');

// The async iterator methods live on a shared prototype, as for any WebIDL
// async iterator, and check their receiver.

const a = new ReadableStream().values();
const b = new ReadableStream()[Symbol.asyncIterator]();
const proto = Object.getPrototypeOf(a);

assert.strictEqual(Object.getPrototypeOf(b), proto);
assert.deepStrictEqual(Reflect.ownKeys(a), []);
assert.deepStrictEqual(Reflect.ownKeys(proto),
                       ['next', 'return', Symbol.toStringTag]);
assert.strictEqual(Object.prototype.toString.call(a),
                   '[object ReadableStream AsyncIterator]');
assert.deepStrictEqual(
  Object.getOwnPropertyDescriptor(proto, Symbol.toStringTag),
  {
    value: 'ReadableStream AsyncIterator',
    writable: false,
    enumerable: false,
    configurable: true,
  });
assert.strictEqual(a[Symbol.asyncIterator](), a);

for (const method of ['next', 'return']) {
  for (const receiver of [undefined, null, 1, {}, new ReadableStream()]) {
    assert.rejects(proto[method].call(receiver), {
      code: 'ERR_INVALID_THIS',
    }).then(common.mustCall());
  }
}

(async () => {
  const rs = new ReadableStream({
    start(c) {
      c.enqueue(1);
      c.enqueue(2);
      c.close();
    },
  });
  const chunks = [];
  for await (const chunk of rs) chunks.push(chunk);
  assert.deepStrictEqual(chunks, [1, 2]);
})().then(common.mustCall());
