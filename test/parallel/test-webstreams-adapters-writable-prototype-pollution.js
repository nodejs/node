'use strict';
const common = require('../common');
const assert = require('assert');
const { Writable } = require('stream');

(async () => {
  const consumed = [];
  const writable = new Writable({
    highWaterMark: 1,
    write: common.mustCall((chunk, encoding, callback) => {
      queueMicrotask(() => {
        consumed.push(chunk[0]);
        callback();
      });
    }, 2),
  });
  writable.on('error', common.mustNotCall());
  const writer = Writable.toWeb(writable).getWriter();
  const fields = ['deferred', 'writeComplete', 'backpressureCleared', 'then'];
  const descriptors = fields.map((field) =>
    Object.getOwnPropertyDescriptor(Object.prototype, field));
  const input = Buffer.alloc(1);

  try {
    for (const field of fields) {
      Object.defineProperty(Object.prototype, field, {
        __proto__: null,
        configurable: true,
        get: common.mustNotCall(`unexpected inherited get: ${field}`),
        set: common.mustNotCall(`unexpected inherited set: ${field}`),
      });
    }
    for (let i = 0; i < 2; i++) {
      input[0] = i;
      await writer.write(input);
      input[0] = 255;
    }
    await writer.close();
  } finally {
    fields.forEach((field, i) => {
      delete Object.prototype[field];
      if (descriptors[i]) {
        Object.defineProperty(Object.prototype, field, descriptors[i]);
      }
    });
  }

  assert.deepStrictEqual(consumed, Array.from({ length: 2 }, (_, i) => i));
  assert.strictEqual(writable.writableFinished, true);
})().then(common.mustCall());
