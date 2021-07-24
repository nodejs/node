'use strict'

const { 
  ReadableStream, 
  WritableStream, 
  TransformStream, 
  pipeline,
  finished,
} = require("stream/web");
const common = require('../common');
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
  const rs = new ReadableStream();

  assert.throws(() => {
    pipeline(rs, () => {});
  }, /ERR_MISSING_ARGS/);

  assert.throws(() => {
    pipeline(() => {});
  }, /ERR_MISSING_ARGS/);
}

{
  const rs = new ReadableStream();
  const ws = new WritableStream();

  assert.throws(() => {
    pipeline(rs, ws, null);
  }, /ERR_INVALID_CALLBACK/);
}

{
  const rs = new ReadableStream();
  const ws = new WritableStream();
  assert.throws(() => {
    pipeline(ws, rs, () => {});
  }, /ERR_INVALID_ARG_TYPE/);
}
  
{
  const rs = new ReadableStream();
  const ws1 = new WritableStream();
  const ws2 = new WritableStream();
  assert.throws(() => {
    pipeline(rs, ws1, ws2, () => {});
  }, /ERR_INVALID_ARG_TYPE/);
}

{
  const rs1 = new ReadableStream();
  const rs2 = new ReadableStream();
  const ws = new WritableStream();
  assert.throws(() => {
    pipeline(rs1, rs2, ws, () => {});
  }, /ERR_INVALID_ARG_TYPE/);
}

{
  const ws = new WritableStream();
  const rs = new ReadableStream({
    start(controller) {
      controller.close();
    },
  });
  
  pipeline(rs, ws, common.mustCall());
}

{
  const ws = new WritableStream();
  const ts = new TransformStream();
  const rs = new ReadableStream({
    start(controller) {
      controller.close();
    },
  });

  pipeline(rs, ts, ws, common.mustCall());
}

{
  const ws = new WritableStream();
  const ts1 = new TransformStream();
  const ts2 = new TransformStream();
  const rs = new ReadableStream({
    start(controller) {
      controller.close();
    },
  });

  pipeline(rs, ts1, ts2, ws, common.mustCall());
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

  pipeline(rs, ws, common.mustCall(() => {
    assert.strictEqual(
      sink.chunks.toString(), 
      'hello'
    );
  }));
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

  pipeline(rs, ts, ws, common.mustCall(() => {
    assert.strictEqual(
      sink.chunks.toString(), 
      'hello'
    );
  }));
}