'use strict';

require('../common');
const assert = require('node:assert');
const { createHistogram, importHistogram } = require('node:perf_hooks');
const kInvalid = { code: 'ERR_INVALID_ARG_VALUE' };

{
  // Data in a SharedArrayBuffer, including a sliced view, imports the same way
  // as data in an ArrayBuffer.
  const h = createHistogram();
  h.record(1);
  h.record(42);
  h.record(1000);
  const exported = h.export();

  for (const offset of [0, 3]) {
    const sab = new SharedArrayBuffer(exported.length + offset + 5);
    new Uint8Array(sab).fill(0xff);
    const view = new Uint8Array(sab, offset, exported.length);
    view.set(exported);
    const imported = importHistogram(view);
    assert.deepStrictEqual(imported.export(), exported);

    // Later writes do not change the imported histogram.
    view.fill(0);
    assert.deepStrictEqual(imported.export(), exported);
    assert.throws(() => importHistogram(view), kInvalid);
  }

  assert.throws(
    () => importHistogram(new Uint8Array(new SharedArrayBuffer(0))), kInvalid);
}
