// Flags: --js-immutable-arraybuffer
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { fork } = require('node:child_process');
const { once } = require('node:events');
const { serialize, deserialize } = require('node:v8');

// Node's custom view serialization records the type and bytes, but omits the
// backing store's immutability. Bare ArrayBuffers already preserve it.
if (process.argv[2] === 'child') {
  process.on('message', common.mustCall((view) => {
    const buffer = view.buffer ?? view;
    process.send({
      immutable: buffer.immutable,
      bytes: [...new Uint8Array(buffer, view.byteOffset ?? 0, view.byteLength)],
    });
    process.disconnect();
  }));
} else {
  const { test } = require('node:test');
  const bytes = [65, 66, 67, 68];

  for (const [name, create] of [
    ['ArrayBuffer', (buffer) => buffer],
    ['Uint8Array', (buffer) => new Uint8Array(buffer)],
    ['DataView', (buffer) => new DataView(buffer)],
    ['Buffer', (buffer) => Buffer.from(buffer)],
  ]) {
    test(`v8 serialization preserves ${name} immutability`, () => {
      const input = create(Uint8Array.from(bytes).buffer.transferToImmutable());
      const output = deserialize(serialize(input));
      const buffer = output.buffer ?? output;
      assert.deepStrictEqual(
        [...new Uint8Array(buffer, output.byteOffset ?? 0, output.byteLength)], bytes);
      assert.strictEqual(buffer.immutable, true);
    });

    test(`advanced IPC preserves ${name} immutability`, async (t) => {
      const child = fork(__filename, ['child'], { serialization: 'advanced' });
      t.after(() => child.kill());
      const message = once(child, 'message');
      const exit = once(child, 'exit');
      child.send(create(Uint8Array.from(bytes).buffer.transferToImmutable()));
      const [output] = await message;
      assert.deepStrictEqual(await exit, [0, null]);
      assert.deepStrictEqual(output.bytes, bytes);
      assert.strictEqual(output.immutable, true);
    });
  }
}
