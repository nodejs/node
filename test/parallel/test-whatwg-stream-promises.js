'use strict';

const common = require('../common');
const {
  ReadableStream,
  WritableStream,
  TransformStream,
  finished: noPromiseFinished,
} = require('stream/web');
const { pipeline, finished } = require('stream/web/promises');
const assert = require('assert');

class Sink {
  constructor() {
    this.chunks = [];
  }
  start() {
    this.started = true;
  }
  write(chunk) {
    this.chunks.push(chunk);
  }
  close() {
    this.closed = true;
  }
  abort() {
    this.aborted = true;
  }
}


{
  const stream = new ReadableStream();

  async function runInner(stream) {
    let ended = false;
    noPromiseFinished(stream, () => {
      ended = true;
    });
    await finished(stream);
    assert(ended);
  }

  async function run(stream) {
    runInner(stream).then(common.mustCall());
  }

  run(stream);
  stream.cancel();
}

{
  const sink = new Sink();
  const ws = new WritableStream(
    sink, { highWaterMark: 1 }
  );

  const data = Buffer.from('hello');
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue(data);
      controller.close();
    },
  });

  const ts = new TransformStream();

  let finished = false;
  noPromiseFinished(ws, () => {
    finished = true;
  });

  pipeline(rs, ts, ws).then(common.mustCall(() => {
    assert.ok(finished);
    assert.strictEqual(sink.chunks.toString(), 'hello');
  }));


}
