'use strict';
const { mustCall } = require('../common');
const { Readable, Duplex } = require('stream');
const assert = require('assert');

function start(controller) {
  controller.enqueue(new Uint8Array(1));
  controller.close();
}

Readable.fromWeb(new ReadableStream({ start }))
.on('data', mustCall((d) => {
  assert.strictEqual(d.length, 1);
}))
.on('end', mustCall())
.resume();

Duplex.fromWeb({
  readable: new ReadableStream({ start }),
  writable: new WritableStream({ write(chunk) {} })
})
.on('data', mustCall((d) => {
  assert.strictEqual(d.length, 1);
}))
.on('end', mustCall())
.resume();
