'use strict';

const common = require('../common');
const assert = require('assert');
const {
  ReadableStream,
  WritableStream,
  TransformStream,
} = require('stream/web');

// Underlying source, sink and transformer methods are read once at
// construction, called as methods of the object they were read from with
// exactly the spec's arguments, and validated up front.

{
  const source = {
    start: common.mustCall(function(...args) {
      assert.strictEqual(this, source);
      assert.strictEqual(args.length, 1);
    }),
    pull: common.mustCall(function(...args) {
      assert.strictEqual(this, source);
      assert.strictEqual(args.length, 1);
      args[0].enqueue('a');
    }),
    cancel: common.mustCall(function(...args) {
      assert.strictEqual(this, source);
      assert.deepStrictEqual(args, ['why']);
    }),
  };
  const rs = new ReadableStream(source);
  source.pull = common.mustNotCall();
  source.cancel = common.mustNotCall();
  const reader = rs.getReader();
  (async () => {
    const { value } = await reader.read();
    assert.strictEqual(value, 'a');
    await reader.cancel('why');
  })().then(common.mustCall());
}

{
  const sink = {
    start: common.mustCall(function(...args) {
      assert.strictEqual(this, sink);
      assert.strictEqual(args.length, 1);
    }),
    write: common.mustCall(function(...args) {
      assert.strictEqual(this, sink);
      assert.strictEqual(args.length, 2);
      assert.strictEqual(args[0], 'a');
    }),
    close: common.mustCall(function(...args) {
      assert.strictEqual(this, sink);
      assert.strictEqual(args.length, 0);
    }),
  };
  const ws = new WritableStream(sink);
  sink.write = common.mustNotCall();
  sink.close = common.mustNotCall();
  const writer = ws.getWriter();
  (async () => {
    await writer.write('a');
    await writer.close();
  })().then(common.mustCall());
}

{
  const sink = {
    abort: common.mustCall(function(...args) {
      assert.strictEqual(this, sink);
      assert.deepStrictEqual(args, ['why']);
    }),
  };
  const ws = new WritableStream(sink);
  sink.abort = common.mustNotCall();
  ws.abort('why').then(common.mustCall());
}

{
  const transformer = {
    transform: common.mustCall(function(...args) {
      assert.strictEqual(this, transformer);
      assert.strictEqual(args.length, 2);
      args[1].enqueue(args[0]);
    }),
    flush: common.mustCall(function(...args) {
      assert.strictEqual(this, transformer);
      assert.strictEqual(args.length, 1);
    }),
  };
  const ts = new TransformStream(transformer);
  transformer.transform = common.mustNotCall();
  transformer.flush = common.mustNotCall();
  const writer = ts.writable.getWriter();
  const reader = ts.readable.getReader();
  writer.write('a');
  writer.close();
  (async () => {
    const { value } = await reader.read();
    assert.strictEqual(value, 'a');
    const { done } = await reader.read();
    assert.strictEqual(done, true);
  })().then(common.mustCall());
}

{
  const transformer = {
    cancel: common.mustCall(function(...args) {
      assert.strictEqual(this, transformer);
      assert.deepStrictEqual(args, ['why']);
    }),
  };
  const ts = new TransformStream(transformer);
  transformer.cancel = common.mustNotCall();
  ts.readable.cancel('why').then(common.mustCall());
}

// A method that is present but not callable is rejected by the constructor.
for (const key of ['pull', 'cancel']) {
  assert.throws(() => new ReadableStream({ [key]: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => new ReadableStream({ type: 'bytes', [key]: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}
for (const key of ['write', 'close', 'abort']) {
  assert.throws(() => new WritableStream({ [key]: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}
for (const key of ['transform', 'flush', 'cancel']) {
  assert.throws(() => new TransformStream({ [key]: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}
