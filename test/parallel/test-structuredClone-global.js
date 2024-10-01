'use strict';

// Flags: --experimental-eventsource --no-experimental-websocket --experimental-websocket

require('../common');
const assert = require('assert');

assert.throws(() => structuredClone(), { code: 'ERR_MISSING_ARGS' });
assert.throws(() => structuredClone(undefined, ''), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => structuredClone(undefined, 1), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => structuredClone(undefined, { transfer: 1 }), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => structuredClone(undefined, { transfer: '' }), { code: 'ERR_INVALID_ARG_TYPE' });

// Options can be null or undefined.
assert.strictEqual(structuredClone(undefined), undefined);
assert.strictEqual(structuredClone(undefined, null), undefined);
// Transfer can be null or undefined.
assert.strictEqual(structuredClone(undefined, { transfer: null }), undefined);
assert.strictEqual(structuredClone(undefined, { }), undefined);

// Transferables or its subclasses should be received with its closest transferable superclass
for (const StreamClass of [ReadableStream, WritableStream, TransformStream]) {
  const original = new StreamClass();
  const transfer = structuredClone(original, { transfer: [original] });
  assert.strictEqual(Object.getPrototypeOf(transfer), StreamClass.prototype);
  assert.ok(transfer instanceof StreamClass);

  const extended = class extends StreamClass {};
  const extendedOriginal = new extended();
  const extendedTransfer = structuredClone(extendedOriginal, { transfer: [extendedOriginal] });
  assert.strictEqual(Object.getPrototypeOf(extendedTransfer), StreamClass.prototype);
  assert.ok(extendedTransfer instanceof StreamClass);
}

// Platform object that is not serializable should throw
[
  { platformClass: Response, brand: 'Response' },
  { platformClass: Request, value: 'http://localhost', brand: 'Request' },
  { platformClass: FormData, brand: 'FormData' },
  { platformClass: MessageEvent, value: 'message', brand: 'MessageEvent' },
  { platformClass: CloseEvent, value: 'dummy type', brand: 'CloseEvent' },
  { platformClass: WebSocket, value: 'http://localhost', brand: 'WebSocket' },
  { platformClass: EventSource, value: 'http://localhost', brand: 'EventSource' },
].forEach((platformEntity) => {
  assert.throws(() => structuredClone(new platformEntity.platformClass(platformEntity.value)),
                new DOMException('Cannot clone object of unsupported type.', 'DataCloneError'),
                `Cloning ${platformEntity.brand} should throw DOMException`);

});

for (const Transferrable of [File, Blob]) {
  const a2 = Transferrable === File ? '' : {};
  const original = new Transferrable([], a2);
  const transfer = structuredClone(original);
  assert.strictEqual(Object.getPrototypeOf(transfer), Transferrable.prototype);
  assert.ok(transfer instanceof Transferrable);

  const extendedOriginal = new (class extends Transferrable {})([], a2);
  const extendedTransfer = structuredClone(extendedOriginal);
  assert.strictEqual(Object.getPrototypeOf(extendedTransfer), Transferrable.prototype);
  assert.ok(extendedTransfer instanceof Transferrable);
}

{
  // See: https://github.com/nodejs/node/issues/49940
  const cloned = structuredClone({}, {
    transfer: {
      *[Symbol.iterator]() {}
    }
  });

  assert.deepStrictEqual(cloned, {});
}

const blob = new Blob();
assert.throws(() => structuredClone(blob, { transfer: [blob] }), { name: 'DataCloneError' });
