'use strict';
const { mustCall } = require('../common');
const { Readable, Duplex } = require('stream');
const { strictEqual } = require('assert');

function start(controller) {
  controller.enqueue(new Uint8Array(1));
  controller.close();
}

Readable.fromWeb(new ReadableStream({ start }))
.on('data', mustCall((d) => {
  strictEqual(d.length, 1);
}))
.on('end', mustCall())
.resume();

Duplex.fromWeb({
  readable: new ReadableStream({ start }),
  writable: new WritableStream({ write(chunk) {} })
})
.on('data', mustCall((d) => {
  strictEqual(d.length, 1);
}))
.on('end', mustCall())
.resume();
