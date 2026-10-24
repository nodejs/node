'use strict';

const common = require('../common');
const assert = require('node:assert');
const { Blob, File } = require('node:buffer');

(async () => {
  for (const create of [
    (parts, options) => new Blob(parts, options),
    (parts, options) => new File(parts, 'test', options),
  ]) {
    const buffer = new ArrayBuffer(16);
    const parts = [
      buffer,
      new Uint8Array(buffer),
      new Uint8Array(buffer, 4, 8),
      new Uint16Array(buffer, 4, 4),
      new DataView(buffer),
      new DataView(buffer, 4, 8),
      Buffer.from(buffer, 4, 8),
    ];
    buffer.transfer();

    for (const part of parts) {
      const empty = create([part]);
      assert.strictEqual(empty.size, 0);
      assert.deepStrictEqual(await empty.arrayBuffer(), new ArrayBuffer(0));

      const mixed = create(['hello', part, 'world']);
      assert.strictEqual(mixed.size, 10);
      assert.strictEqual(await mixed.text(), 'helloworld');
    }

    // Options conversion can detach buffers after blob parts conversion.
    const anotherBuffer = new ArrayBuffer(16);
    const view = new DataView(anotherBuffer, 4, 8);
    const blob = create([anotherBuffer, view, 'hello'], {
      get endings() {
        anotherBuffer.transfer();
        return 'transparent';
      },
    });
    assert.strictEqual(blob.size, 5);
    assert.strictEqual(await blob.text(), 'hello');
  }
})().then(common.mustCall());
