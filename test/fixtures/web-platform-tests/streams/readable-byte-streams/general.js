'use strict';

if (self.importScripts) {
  self.importScripts('../resources/rs-utils.js');
  self.importScripts('../resources/test-utils.js');
  self.importScripts('/resources/testharness.js');
}

const error1 = new Error('error1');
error1.name = 'error1';

test(() => {
  assert_throws(new TypeError(), () => new ReadableStream().getReader({ mode: 'byob' }));
}, 'getReader({mode: "byob"}) throws on non-bytes streams');


test(() => {
  // Constructing ReadableStream with an empty underlying byte source object as parameter shouldn't throw.
  new ReadableStream({ type: 'bytes' }).getReader({ mode: 'byob' });
  // Constructor must perform ToString(type).
  new ReadableStream({ type: { toString() {return 'bytes';} } })
    .getReader({ mode: 'byob' });
  new ReadableStream({ type: { toString: null, valueOf() {return 'bytes';} } })
    .getReader({ mode: 'byob' });
}, 'ReadableStream with byte source can be constructed with no errors');

test(() => {
  const ReadableStreamBYOBReader = new ReadableStream({ type: 'bytes' }).getReader({ mode: 'byob' }).constructor;
  const rs = new ReadableStream({ type: 'bytes' });

  let reader = rs.getReader({ mode: { toString() { return 'byob'; } } });
  assert_true(reader instanceof ReadableStreamBYOBReader, 'must give a BYOB reader');
  reader.releaseLock();

  reader = rs.getReader({ mode: { toString: null, valueOf() {return 'byob';} } });
  assert_true(reader instanceof ReadableStreamBYOBReader, 'must give a BYOB reader');
  reader.releaseLock();

  reader = rs.getReader({ mode: 'byob', notmode: 'ignored' });
  assert_true(reader instanceof ReadableStreamBYOBReader, 'must give a BYOB reader');
}, 'getReader({mode}) must perform ToString()');

promise_test(() => {
  let startCalled = false;
  let startCalledBeforePull = false;
  let desiredSize;
  let controller;

  let resolveTestPromise;
  const testPromise = new Promise(resolve => {
    resolveTestPromise = resolve;
  });

  new ReadableStream({
    start(c) {
      controller = c;
      startCalled = true;
    },
    pull() {
      startCalledBeforePull = startCalled;
      desiredSize = controller.desiredSize;
      resolveTestPromise();
    },
    type: 'bytes'
  }, {
    highWaterMark: 256
  });

  return testPromise.then(() => {
    assert_true(startCalledBeforePull, 'start should be called before pull');
    assert_equals(desiredSize, 256, 'desiredSize should equal highWaterMark');
  });

}, 'ReadableStream with byte source: Construct and expect start and pull being called');

promise_test(() => {
  let pullCount = 0;
  let checkedNoPull = false;

  let resolveTestPromise;
  const testPromise = new Promise(resolve => {
    resolveTestPromise = resolve;
  });
  let resolveStartPromise;

  new ReadableStream({
    start() {
      return new Promise(resolve => {
        resolveStartPromise = resolve;
      });
    },
    pull() {
      if (checkedNoPull) {
        resolveTestPromise();
      }

      ++pullCount;
    },
    type: 'bytes'
  }, {
    highWaterMark: 256
  });

  Promise.resolve().then(() => {
    assert_equals(pullCount, 0);
    checkedNoPull = true;
    resolveStartPromise();
  });

  return testPromise;

}, 'ReadableStream with byte source: No automatic pull call if start doesn\'t finish');

promise_test(t => {
  new ReadableStream({
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  }, {
    highWaterMark: 0
  });

  return Promise.resolve();
}, 'ReadableStream with byte source: Construct with highWaterMark of 0');

test(() => {
  new ReadableStream({
    start(c) {
      assert_equals(c.desiredSize, 10, 'desiredSize must start at the highWaterMark');
      c.close();
      assert_equals(c.desiredSize, 0, 'after closing, desiredSize must be 0');
    },
    type: 'bytes'
  }, {
    highWaterMark: 10
  });
}, 'ReadableStream with byte source: desiredSize when closed');

test(() => {
  new ReadableStream({
    start(c) {
      assert_equals(c.desiredSize, 10, 'desiredSize must start at the highWaterMark');
      c.error();
      assert_equals(c.desiredSize, null, 'after erroring, desiredSize must be null');
    },
    type: 'bytes'
  }, {
    highWaterMark: 10
  });
}, 'ReadableStream with byte source: desiredSize when errored');

promise_test(t => {
  const stream = new ReadableStream({
    type: 'bytes'
  });

  const reader = stream.getReader();
  reader.releaseLock();

  return promise_rejects(t, new TypeError(), reader.closed, 'closed must reject');
}, 'ReadableStream with byte source: getReader(), then releaseLock()');

promise_test(t => {
  const stream = new ReadableStream({
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });
  reader.releaseLock();

  return promise_rejects(t, new TypeError(), reader.closed, 'closed must reject');
}, 'ReadableStream with byte source: getReader() with mode set to byob, then releaseLock()');

promise_test(t => {
  const stream = new ReadableStream({
    start(c) {
      c.close();
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader();

  return reader.closed.then(() => {
    assert_throws(new TypeError(), () => stream.getReader(), 'getReader() must throw');
  });
}, 'ReadableStream with byte source: Test that closing a stream does not release a reader automatically');

promise_test(t => {
  const stream = new ReadableStream({
    start(c) {
      c.close();
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.closed.then(() => {
    assert_throws(new TypeError(), () => stream.getReader({ mode: 'byob' }), 'getReader() must throw');
  });
}, 'ReadableStream with byte source: Test that closing a stream does not release a BYOB reader automatically');

promise_test(t => {
  const stream = new ReadableStream({
    start(c) {
      c.error(error1);
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader();

  return promise_rejects(t, error1, reader.closed, 'closed must reject').then(() => {
    assert_throws(new TypeError(), () => stream.getReader(), 'getReader() must throw');
  });
}, 'ReadableStream with byte source: Test that erroring a stream does not release a reader automatically');

promise_test(t => {
  const stream = new ReadableStream({
    start(c) {
      c.error(error1);
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return promise_rejects(t, error1, reader.closed, 'closed must reject').then(() => {
    assert_throws(new TypeError(), () => stream.getReader({ mode: 'byob' }), 'getReader() must throw');
  });
}, 'ReadableStream with byte source: Test that erroring a stream does not release a BYOB reader automatically');

test(() => {
  const stream = new ReadableStream({
    type: 'bytes'
  });

  const reader = stream.getReader();
  reader.read();
  assert_throws(new TypeError(), () => reader.releaseLock(), 'reader.releaseLock() must throw');
}, 'ReadableStream with byte source: releaseLock() on ReadableStreamReader with pending read() must throw');

promise_test(() => {
  let pullCount = 0;

  const stream = new ReadableStream({
    pull() {
      ++pullCount;
    },
    type: 'bytes'
  }, {
    highWaterMark: 8
  });

  stream.getReader();

  assert_equals(pullCount, 0, 'No pull as start() just finished and is not yet reflected to the state of the stream');

  return Promise.resolve().then(() => {
    assert_equals(pullCount, 1, 'pull must be invoked');
  });
}, 'ReadableStream with byte source: Automatic pull() after start()');

promise_test(() => {
  let pullCount = 0;

  const stream = new ReadableStream({
    pull() {
      ++pullCount;
    },
    type: 'bytes'
  }, {
    highWaterMark: 0
  });

  const reader = stream.getReader();
  reader.read();

  assert_equals(pullCount, 0, 'No pull as start() just finished and is not yet reflected to the state of the stream');

  return Promise.resolve().then(() => {
    assert_equals(pullCount, 1, 'pull must be invoked');
  });
}, 'ReadableStream with byte source: Automatic pull() after start() and read()');

// View buffers are detached after pull() returns, so record the information at the time that pull() was called.
function extractViewInfo(view) {
  return {
    constructor: view.constructor,
    bufferByteLength: view.buffer.byteLength,
    byteOffset: view.byteOffset,
    byteLength: view.byteLength
  };
}

promise_test(() => {
  let pullCount = 0;
  let controller;
  const byobRequests = [];

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      const byobRequest = controller.byobRequest;
      const view = byobRequest.view;
      byobRequests[pullCount] = {
        defined: byobRequest !== undefined,
        viewDefined: view !== undefined,
        viewInfo: extractViewInfo(view)
      };
      if (pullCount === 0) {
        view[0] = 0x01;
        byobRequest.respond(1);
      } else if (pullCount === 1) {
        view[0] = 0x02;
        view[1] = 0x03;
        byobRequest.respond(2);
      }

      ++pullCount;
    },
    type: 'bytes',
    autoAllocateChunkSize: 16
  }, {
    highWaterMark: 0
  });

  const reader = stream.getReader();
  const p0 = reader.read();
  const p1 = reader.read();

  assert_equals(pullCount, 0, 'No pull() as start() just finished and is not yet reflected to the state of the stream');

  return Promise.resolve().then(() => {
    assert_equals(pullCount, 1, 'pull() must have been invoked once');
    const byobRequest = byobRequests[0];
    assert_true(byobRequest.defined, 'first byobRequest must not be undefined');
    assert_true(byobRequest.viewDefined, 'first byobRequest.view must not be undefined');
    const viewInfo = byobRequest.viewInfo;
    assert_equals(viewInfo.constructor, Uint8Array, 'first view.constructor should be Uint8Array');
    assert_equals(viewInfo.bufferByteLength, 16, 'first view.buffer.byteLength should be 16');
    assert_equals(viewInfo.byteOffset, 0, 'first view.byteOffset should be 0');
    assert_equals(viewInfo.byteLength, 16, 'first view.byteLength should be 16');

    return p0;
  }).then(result => {
    assert_equals(pullCount, 2, 'pull() must have been invoked twice');
    const value = result.value;
    assert_not_equals(value, undefined, 'first read should have a value');
    assert_equals(value.constructor, Uint8Array, 'first value should be a Uint8Array');
    assert_equals(value.buffer.byteLength, 16, 'first value.buffer.byteLength should be 16');
    assert_equals(value.byteOffset, 0, 'first value.byteOffset should be 0');
    assert_equals(value.byteLength, 1, 'first value.byteLength should be 1');
    assert_equals(value[0], 0x01, 'first value[0] should be 0x01');
    const byobRequest = byobRequests[1];
    assert_true(byobRequest.defined, 'second byobRequest must not be undefined');
    assert_true(byobRequest.viewDefined, 'second byobRequest.view must not be undefined');
    const viewInfo = byobRequest.viewInfo;
    assert_equals(viewInfo.constructor, Uint8Array, 'second view.constructor should be Uint8Array');
    assert_equals(viewInfo.bufferByteLength, 16, 'second view.buffer.byteLength should be 16');
    assert_equals(viewInfo.byteOffset, 0, 'second view.byteOffset should be 0');
    assert_equals(viewInfo.byteLength, 16, 'second view.byteLength should be 16');

    return p1;
  }).then(result => {
    assert_equals(pullCount, 2, 'pull() should only be invoked twice');
    const value = result.value;
    assert_not_equals(value, undefined, 'second read should have a value');
    assert_equals(value.constructor, Uint8Array, 'second value should be a Uint8Array');
    assert_equals(value.buffer.byteLength, 16, 'second value.buffer.byteLength should be 16');
    assert_equals(value.byteOffset, 0, 'second value.byteOffset should be 0');
    assert_equals(value.byteLength, 2, 'second value.byteLength should be 2');
    assert_equals(value[0], 0x02, 'second value[0] should be 0x02');
    assert_equals(value[1], 0x03, 'second value[1] should be 0x03');
  });
}, 'ReadableStream with byte source: autoAllocateChunkSize');

promise_test(() => {
  let pullCount = 0;
  let controller;
  const byobRequests = [];

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      const byobRequest = controller.byobRequest;
      const view = byobRequest.view;
      byobRequests[pullCount] = {
        defined: byobRequest !== undefined,
        viewDefined: view !== undefined,
        viewInfo: extractViewInfo(view)
      };
      if (pullCount === 0) {
        view[0] = 0x01;
        byobRequest.respond(1);
      } else if (pullCount === 1) {
        view[0] = 0x02;
        view[1] = 0x03;
        byobRequest.respond(2);
      }

      ++pullCount;
    },
    type: 'bytes',
    autoAllocateChunkSize: 16
  }, {
    highWaterMark: 0
  });

  const reader = stream.getReader();
  return reader.read().then(result => {
    const value = result.value;
    assert_not_equals(value, undefined, 'first read should have a value');
    assert_equals(value.constructor, Uint8Array, 'first value should be a Uint8Array');
    assert_equals(value.buffer.byteLength, 16, 'first value.buffer.byteLength should be 16');
    assert_equals(value.byteOffset, 0, 'first value.byteOffset should be 0');
    assert_equals(value.byteLength, 1, 'first value.byteLength should be 1');
    assert_equals(value[0], 0x01, 'first value[0] should be 0x01');
    const byobRequest = byobRequests[0];
    assert_true(byobRequest.defined, 'first byobRequest must not be undefined');
    assert_true(byobRequest.viewDefined, 'first byobRequest.view must not be undefined');
    const viewInfo = byobRequest.viewInfo;
    assert_equals(viewInfo.constructor, Uint8Array, 'first view.constructor should be Uint8Array');
    assert_equals(viewInfo.bufferByteLength, 16, 'first view.buffer.byteLength should be 16');
    assert_equals(viewInfo.byteOffset, 0, 'first view.byteOffset should be 0');
    assert_equals(viewInfo.byteLength, 16, 'first view.byteLength should be 16');

    reader.releaseLock();
    const byobReader = stream.getReader({ mode: 'byob' });
    return byobReader.read(new Uint8Array(32));
  }).then(result => {
    const value = result.value;
    assert_not_equals(value, undefined, 'second read should have a value');
    assert_equals(value.constructor, Uint8Array, 'second value should be a Uint8Array');
    assert_equals(value.buffer.byteLength, 32, 'second value.buffer.byteLength should be 32');
    assert_equals(value.byteOffset, 0, 'second value.byteOffset should be 0');
    assert_equals(value.byteLength, 2, 'second value.byteLength should be 2');
    assert_equals(value[0], 0x02, 'second value[0] should be 0x02');
    assert_equals(value[1], 0x03, 'second value[1] should be 0x03');
    const byobRequest = byobRequests[1];
    assert_true(byobRequest.defined, 'second byobRequest must not be undefined');
    assert_true(byobRequest.viewDefined, 'second byobRequest.view must not be undefined');
    const viewInfo = byobRequest.viewInfo;
    assert_equals(viewInfo.constructor, Uint8Array, 'second view.constructor should be Uint8Array');
    assert_equals(viewInfo.bufferByteLength, 32, 'second view.buffer.byteLength should be 32');
    assert_equals(viewInfo.byteOffset, 0, 'second view.byteOffset should be 0');
    assert_equals(viewInfo.byteLength, 32, 'second view.byteLength should be 32');
    assert_equals(pullCount, 2, 'pullCount should be 2');
  });
}, 'ReadableStream with byte source: Mix of auto allocate and BYOB');

promise_test(() => {
  let pullCount = 0;

  const stream = new ReadableStream({
    pull() {
      ++pullCount;
    },
    type: 'bytes'
  }, {
    highWaterMark: 0
  });

  const reader = stream.getReader();
  reader.read(new Uint8Array(8));

  assert_equals(pullCount, 0, 'No pull as start() just finished and is not yet reflected to the state of the stream');

  return Promise.resolve().then(() => {
    assert_equals(pullCount, 1, 'pull must be invoked');
  });
}, 'ReadableStream with byte source: Automatic pull() after start() and read(view)');

promise_test(() => {
  let pullCount = 0;

  let controller;
  let desiredSizeInStart;
  let desiredSizeInPull;

  const stream = new ReadableStream({
    start(c) {
      c.enqueue(new Uint8Array(16));
      desiredSizeInStart = c.desiredSize;
      controller = c;
    },
    pull() {
      ++pullCount;

      if (pullCount === 1) {
        desiredSizeInPull = controller.desiredSize;
      }
    },
    type: 'bytes'
  }, {
    highWaterMark: 8
  });

  return Promise.resolve().then(() => {
    assert_equals(pullCount, 0, 'No pull as the queue was filled by start()');
    assert_equals(desiredSizeInStart, -8, 'desiredSize after enqueue() in start()');

    const reader = stream.getReader();

    const promise = reader.read();
    assert_equals(pullCount, 1, 'The first pull() should be made on read()');
    assert_equals(desiredSizeInPull, 8, 'desiredSize in pull()');

    return promise.then(result => {
      assert_equals(result.done, false, 'result.done');

      const view = result.value;
      assert_equals(view.constructor, Uint8Array, 'view.constructor');
      assert_equals(view.buffer.byteLength, 16, 'view.buffer');
      assert_equals(view.byteOffset, 0, 'view.byteOffset');
      assert_equals(view.byteLength, 16, 'view.byteLength');
    });
  });
}, 'ReadableStream with byte source: enqueue(), getReader(), then read()');

promise_test(() => {
  let controller;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    type: 'bytes'
  });

  const reader = stream.getReader();

  const promise = reader.read().then(result => {
    assert_equals(result.done, false);

    const view = result.value;
    assert_equals(view.constructor, Uint8Array);
    assert_equals(view.buffer.byteLength, 1);
    assert_equals(view.byteOffset, 0);
    assert_equals(view.byteLength, 1);
  });

  controller.enqueue(new Uint8Array(1));

  return promise;
}, 'ReadableStream with byte source: Push source that doesn\'t understand pull signal');

test(() => {
  assert_throws(new TypeError(), () => new ReadableStream({
    pull: 'foo',
    type: 'bytes'
  }), 'constructor should throw');
}, 'ReadableStream with byte source: pull() function is not callable');

promise_test(() => {
  const stream = new ReadableStream({
    start(c) {
      c.enqueue(new Uint16Array(16));
    },
    type: 'bytes'
  });

  const reader = stream.getReader();

  return reader.read().then(result => {
    assert_equals(result.done, false);

    const view = result.value;
    assert_equals(view.constructor, Uint8Array);
    assert_equals(view.buffer.byteLength, 32);
    assert_equals(view.byteOffset, 0);
    assert_equals(view.byteLength, 32);
  });
}, 'ReadableStream with byte source: enqueue() with Uint16Array, getReader(), then read()');

promise_test(t => {
  const stream = new ReadableStream({
    start(c) {
      const view = new Uint8Array(16);
      view[0] = 0x01;
      view[8] = 0x02;
      c.enqueue(view);
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const byobReader = stream.getReader({ mode: 'byob' });

  return byobReader.read(new Uint8Array(8)).then(result => {
    assert_equals(result.done, false, 'done');

    const view = result.value;
    assert_equals(view.constructor, Uint8Array, 'value.constructor');
    assert_equals(view.buffer.byteLength, 8, 'value.buffer.byteLength');
    assert_equals(view.byteOffset, 0, 'value.byteOffset');
    assert_equals(view.byteLength, 8, 'value.byteLength');
    assert_equals(view[0], 0x01);

    byobReader.releaseLock();

    const reader = stream.getReader();

    return reader.read();
  }).then(result => {
    assert_equals(result.done, false, 'done');

    const view = result.value;
    assert_equals(view.constructor, Uint8Array, 'value.constructor');
    assert_equals(view.buffer.byteLength, 16, 'value.buffer.byteLength');
    assert_equals(view.byteOffset, 8, 'value.byteOffset');
    assert_equals(view.byteLength, 8, 'value.byteLength');
    assert_equals(view[0], 0x02);
  });
}, 'ReadableStream with byte source: enqueue(), read(view) partially, then read()');

promise_test(t => {
  let controller;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader();

  controller.enqueue(new Uint8Array(16));
  controller.close();

  return reader.read().then(result => {
    assert_equals(result.done, false, 'done');

    const view = result.value;
    assert_equals(view.byteOffset, 0, 'byteOffset');
    assert_equals(view.byteLength, 16, 'byteLength');

    return reader.read();
  }).then(result => {
    assert_equals(result.done, true, 'done');
    assert_equals(result.value, undefined, 'value');
  });
}, 'ReadableStream with byte source: getReader(), enqueue(), close(), then read()');

promise_test(t => {
  const stream = new ReadableStream({
    start(c) {
      c.enqueue(new Uint8Array(16));
      c.close();
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader();

  return reader.read().then(result => {
    assert_equals(result.done, false, 'done');

    const view = result.value;
    assert_equals(view.byteOffset, 0, 'byteOffset');
    assert_equals(view.byteLength, 16, 'byteLength');

    return reader.read();
  }).then(result => {
    assert_equals(result.done, true, 'done');
    assert_equals(result.value, undefined, 'value');
  });
}, 'ReadableStream with byte source: enqueue(), close(), getReader(), then read()');

promise_test(() => {
  let controller;
  let byobRequest;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      controller.enqueue(new Uint8Array(16));
      byobRequest = controller.byobRequest;
    },
    type: 'bytes'
  });

  const reader = stream.getReader();

  return reader.read().then(result => {
    assert_equals(result.done, false, 'done');
    assert_equals(result.value.byteLength, 16, 'byteLength');
    assert_equals(byobRequest, undefined, 'byobRequest must be undefined');
  });
}, 'ReadableStream with byte source: Respond to pull() by enqueue()');

promise_test(() => {
  let pullCount = 0;

  let controller;
  let byobRequest;
  const desiredSizes = [];

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      byobRequest = controller.byobRequest;
      desiredSizes.push(controller.desiredSize);
      controller.enqueue(new Uint8Array(1));
      desiredSizes.push(controller.desiredSize);
      controller.enqueue(new Uint8Array(1));
      desiredSizes.push(controller.desiredSize);

      ++pullCount;
    },
    type: 'bytes'
  }, {
    highWaterMark: 0
  });

  const reader = stream.getReader();

  const p0 = reader.read();
  const p1 = reader.read();
  const p2 = reader.read();

  // Respond to the first pull call.
  controller.enqueue(new Uint8Array(1));

  assert_equals(pullCount, 0, 'pullCount after the enqueue() outside pull');

  return Promise.all([p0, p1, p2]).then(result => {
    assert_equals(pullCount, 1, 'pullCount after completion of all read()s');

    assert_equals(result[0].done, false, 'result[0].done');
    assert_equals(result[0].value.byteLength, 1, 'result[0].value.byteLength');
    assert_equals(result[1].done, false, 'result[1].done');
    assert_equals(result[1].value.byteLength, 1, 'result[1].value.byteLength');
    assert_equals(result[2].done, false, 'result[2].done');
    assert_equals(result[2].value.byteLength, 1, 'result[2].value.byteLength');
    assert_equals(byobRequest, undefined, 'byobRequest should be undefined');
    assert_equals(desiredSizes[0], 0, 'desiredSize on pull should be 0');
    assert_equals(desiredSizes[1], 0, 'desiredSize after 1st enqueue() should be 0');
    assert_equals(desiredSizes[2], 0, 'desiredSize after 2nd enqueue() should be 0');
    assert_equals(pullCount, 1, 'pull() should only be called once');
  });
}, 'ReadableStream with byte source: Respond to pull() by enqueue() asynchronously');

promise_test(() => {
  let pullCount = 0;

  let byobRequest;
  const desiredSizes = [];

  const stream = new ReadableStream({
    pull(c) {
      byobRequest = c.byobRequest;
      desiredSizes.push(c.desiredSize);

      if (pullCount < 3) {
        c.enqueue(new Uint8Array(1));
      } else {
        c.close();
      }

      ++pullCount;
    },
    type: 'bytes'
  }, {
    highWaterMark: 256
  });

  const reader = stream.getReader();

  const p0 = reader.read();
  const p1 = reader.read();
  const p2 = reader.read();

  assert_equals(pullCount, 0, 'No pull as start() just finished and is not yet reflected to the state of the stream');

  return Promise.all([p0, p1, p2]).then(result => {
    assert_equals(pullCount, 4, 'pullCount after completion of all read()s');

    assert_equals(result[0].done, false, 'result[0].done');
    assert_equals(result[0].value.byteLength, 1, 'result[0].value.byteLength');
    assert_equals(result[1].done, false, 'result[1].done');
    assert_equals(result[1].value.byteLength, 1, 'result[1].value.byteLength');
    assert_equals(result[2].done, false, 'result[2].done');
    assert_equals(result[2].value.byteLength, 1, 'result[2].value.byteLength');
    assert_equals(byobRequest, undefined, 'byobRequest should be undefined');
    assert_equals(desiredSizes[0], 256, 'desiredSize on pull should be 256');
    assert_equals(desiredSizes[1], 256, 'desiredSize after 1st enqueue() should be 256');
    assert_equals(desiredSizes[2], 256, 'desiredSize after 2nd enqueue() should be 256');
    assert_equals(desiredSizes[3], 256, 'desiredSize after 3rd enqueue() should be 256');
  });
}, 'ReadableStream with byte source: Respond to multiple pull() by separate enqueue()');

promise_test(() => {
  let controller;

  let pullCount = 0;
  const byobRequestDefined = [];

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      byobRequestDefined.push(controller.byobRequest !== undefined);

      const view = controller.byobRequest.view;
      view[0] = 0x01;
      controller.byobRequest.respond(1);

      byobRequestDefined.push(controller.byobRequest !== undefined);

      ++pullCount;
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.read(new Uint8Array(1)).then(result => {
    assert_equals(result.done, false, 'result.done');
    assert_equals(result.value.byteLength, 1, 'result.value.byteLength');
    assert_equals(result.value[0], 0x01, 'result.value[0]');
    assert_equals(pullCount, 1, 'pull() should be called only once');
    assert_true(byobRequestDefined[0], 'byobRequest must not be undefined before respond()');
    assert_false(byobRequestDefined[1], 'byobRequest must be undefined after respond()');
  });
}, 'ReadableStream with byte source: read(view), then respond()');

promise_test(() => {
  let controller;

  let pullCount = 0;
  const byobRequestDefined = [];

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      byobRequestDefined.push(controller.byobRequest !== undefined);

      // Emulate ArrayBuffer transfer by just creating a new ArrayBuffer and pass it. By checking the result of
      // read(view), we test that the respond()'s buffer argument is working correctly.
      //
      // A real implementation of the underlying byte source would transfer controller.byobRequest.view.buffer into
      // a new ArrayBuffer, then construct a view around it and write to it.
      const transferredView = new Uint8Array(1);
      transferredView[0] = 0x01;
      controller.byobRequest.respondWithNewView(transferredView);

      byobRequestDefined.push(controller.byobRequest !== undefined);

      ++pullCount;
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.read(new Uint8Array(1)).then(result => {
    assert_equals(result.done, false, 'result.done');
    assert_equals(result.value.byteLength, 1, 'result.value.byteLength');
    assert_equals(result.value[0], 0x01, 'result.value[0]');
    assert_equals(pullCount, 1, 'pull() should be called only once');
    assert_true(byobRequestDefined[0], 'byobRequest must not be undefined before respond()');
    assert_false(byobRequestDefined[1], 'byobRequest must be undefined after respond()');
  });
}, 'ReadableStream with byte source: read(view), then respond() with a transferred ArrayBuffer');

promise_test(() => {
  let controller;
  let byobRequestWasDefined;
  let incorrectRespondException;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      byobRequestWasDefined = controller.byobRequest !== undefined;

      try {
        controller.byobRequest.respond(2);
      } catch (e) {
        incorrectRespondException = e;
      }

      controller.byobRequest.respond(1);
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.read(new Uint8Array(1)).then(() => {
    assert_true(byobRequestWasDefined, 'byobRequest should be defined');
    assert_not_equals(incorrectRespondException, undefined, 'respond() must throw');
    assert_equals(incorrectRespondException.name, 'RangeError', 'respond() must throw a RangeError');
  });
}, 'ReadableStream with byte source: read(view), then respond() with too big value');

promise_test(() => {
  let pullCount = 0;

  let controller;
  let byobRequest;
  let viewInfo;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      ++pullCount;

      byobRequest = controller.byobRequest;
      const view = byobRequest.view;
      viewInfo = extractViewInfo(view);

      view[0] = 0x01;
      view[1] = 0x02;
      view[2] = 0x03;

      controller.byobRequest.respond(3);
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.read(new Uint16Array(2)).then(result => {
    assert_equals(pullCount, 1);

    assert_equals(result.done, false, 'done');

    const view = result.value;
    assert_equals(view.byteOffset, 0, 'byteOffset');
    assert_equals(view.byteLength, 2, 'byteLength');

    assert_equals(view[0], 0x0201);

    return reader.read(new Uint8Array(1));
  }).then(result => {
    assert_equals(pullCount, 1);
    assert_not_equals(byobRequest, undefined, 'byobRequest must not be undefined');
    assert_equals(viewInfo.constructor, Uint8Array, 'view.constructor should be Uint8Array');
    assert_equals(viewInfo.bufferByteLength, 4, 'view.buffer.byteLength should be 4');
    assert_equals(viewInfo.byteOffset, 0, 'view.byteOffset should be 0');
    assert_equals(viewInfo.byteLength, 4, 'view.byteLength should be 4');

    assert_equals(result.done, false, 'done');

    const view = result.value;
    assert_equals(view.byteOffset, 0, 'byteOffset');
    assert_equals(view.byteLength, 1, 'byteLength');

    assert_equals(view[0], 0x03);
  });
}, 'ReadableStream with byte source: respond(3) to read(view) with 2 element Uint16Array enqueues the 1 byte ' +
   'remainder');

promise_test(t => {
  const stream = new ReadableStream({
    start(controller) {
      const view = new Uint8Array(16);
      view[15] = 0x01;
      controller.enqueue(view);
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.read(new Uint8Array(16)).then(result => {
    assert_equals(result.done, false);

    const view = result.value;
    assert_equals(view.byteOffset, 0);
    assert_equals(view.byteLength, 16);
    assert_equals(view[15], 0x01);
  });
}, 'ReadableStream with byte source: enqueue(), getReader(), then read(view)');

promise_test(t => {
  let cancelCount = 0;
  let reason;

  const passedReason = new TypeError('foo');

  const stream = new ReadableStream({
    start(c) {
      c.enqueue(new Uint8Array(16));
    },
    pull: t.unreached_func('pull() should not be called'),
    cancel(r) {
      if (cancelCount === 0) {
        reason = r;
      }

      ++cancelCount;
    },
    type: 'bytes'
  });

  const reader = stream.getReader();

  return reader.cancel(passedReason).then(result => {
    assert_equals(result, undefined);
    assert_equals(cancelCount, 1);
    assert_equals(reason, passedReason, 'reason should equal the passed reason');
  });
}, 'ReadableStream with byte source: enqueue(), getReader(), then cancel() (mode = not BYOB)');

promise_test(t => {
  let cancelCount = 0;
  let reason;

  const passedReason = new TypeError('foo');

  const stream = new ReadableStream({
    start(c) {
      c.enqueue(new Uint8Array(16));
    },
    pull: t.unreached_func('pull() should not be called'),
    cancel(r) {
      if (cancelCount === 0) {
        reason = r;
      }

      ++cancelCount;
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.cancel(passedReason).then(result => {
    assert_equals(result, undefined);
    assert_equals(cancelCount, 1);
    assert_equals(reason, passedReason, 'reason should equal the passed reason');
  });
}, 'ReadableStream with byte source: enqueue(), getReader(), then cancel() (mode = BYOB)');

promise_test(t => {
  let cancelCount = 0;
  let reason;

  const passedReason = new TypeError('foo');

  let controller;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull: t.unreached_func('pull() should not be called'),
    cancel(r) {
      if (cancelCount === 0) {
        reason = r;
        controller.byobRequest.respond(0);
      }

      ++cancelCount;

      return 'bar';
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  const readPromise = reader.read(new Uint8Array(1)).then(result => {
    assert_equals(result.done, true);
  });

  const cancelPromise = reader.cancel(passedReason).then(result => {
    assert_equals(result, undefined);
    assert_equals(cancelCount, 1);
    assert_equals(reason, passedReason, 'reason should equal the passed reason');
  });

  return Promise.all([readPromise, cancelPromise]);
}, 'ReadableStream with byte source: getReader(), read(view), then cancel()');

promise_test(() => {
  let pullCount = 0;

  let controller;
  let byobRequest;
  const viewInfos = [];

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      byobRequest = controller.byobRequest;

      viewInfos.push(extractViewInfo(controller.byobRequest.view));
      controller.enqueue(new Uint8Array(1));
      viewInfos.push(extractViewInfo(controller.byobRequest.view));

      ++pullCount;
    },
    type: 'bytes'
  });

  return Promise.resolve().then(() => {
    assert_equals(pullCount, 0, 'No pull() as no read(view) yet');

    const reader = stream.getReader({ mode: 'byob' });

    const promise = reader.read(new Uint16Array(1)).then(result => {
      assert_equals(result.done, true, 'result.done');
      assert_equals(result.value.constructor, Uint16Array, 'result.value');
    });

    assert_equals(pullCount, 1, '1 pull() should have been made in response to partial fill by enqueue()');
    assert_not_equals(byobRequest, undefined, 'byobRequest should not be undefined');
    assert_equals(viewInfos[0].byteLength, 2, 'byteLength before enqueue() shouild be 2');
    assert_equals(viewInfos[1].byteLength, 1, 'byteLength after enqueue() should be 1');


    reader.cancel();

    // Tell that the buffer given via pull() is returned.
    controller.byobRequest.respond(0);

    assert_equals(pullCount, 1, 'pull() should only be called once');
    return promise;
  });
}, 'ReadableStream with byte source: cancel() with partially filled pending pull() request');

promise_test(() => {
  let controller;
  let byobRequest;

  const stream = new ReadableStream({
    start(c) {
      const view = new Uint8Array(8);
      view[7] = 0x01;
      c.enqueue(view);

      controller = c;
    },
    pull() {
      byobRequest = controller.byobRequest;
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  const buffer = new ArrayBuffer(16);

  return reader.read(new Uint8Array(buffer, 8, 8)).then(result => {
    assert_equals(result.done, false);

    assert_equals(byobRequest, undefined, 'byobRequest must be undefined');

    const view = result.value;
    assert_equals(view.constructor, Uint8Array);
    assert_equals(view.buffer.byteLength, 16);
    assert_equals(view.byteOffset, 8);
    assert_equals(view.byteLength, 8);
    assert_equals(view[7], 0x01);
  });
}, 'ReadableStream with byte source: enqueue(), getReader(), then read(view) where view.buffer is not fully ' +
   'covered by view');

promise_test(() => {
  let controller;
  let byobRequest;

  const stream = new ReadableStream({
    start(c) {
      let view;

      view = new Uint8Array(16);
      view[15] = 123;
      c.enqueue(view);

      view = new Uint8Array(8);
      view[7] = 111;
      c.enqueue(view);

      controller = c;
    },
    pull() {
      byobRequest = controller.byobRequest;
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.read(new Uint8Array(24)).then(result => {
    assert_equals(result.done, false, 'done');

    assert_equals(byobRequest, undefined, 'byobRequest must be undefined');

    const view = result.value;
    assert_equals(view.byteOffset, 0, 'byteOffset');
    assert_equals(view.byteLength, 24, 'byteLength');
    assert_equals(view[15], 123, 'Contents are set from the first chunk');
    assert_equals(view[23], 111, 'Contents are set from the second chunk');
  });
}, 'ReadableStream with byte source: Multiple enqueue(), getReader(), then read(view)');

promise_test(() => {
  let byobRequest;

  const stream = new ReadableStream({
    start(c) {
      const view = new Uint8Array(16);
      view[15] = 0x01;
      c.enqueue(view);
    },
    pull(controller) {
      byobRequest = controller.byobRequest;
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.read(new Uint8Array(24)).then(result => {
    assert_equals(result.done, false);

    assert_equals(byobRequest, undefined, 'byobRequest must be undefined');

    const view = result.value;
    assert_equals(view.byteOffset, 0);
    assert_equals(view.byteLength, 16);
    assert_equals(view[15], 0x01);
  });
}, 'ReadableStream with byte source: enqueue(), getReader(), then read(view) with a bigger view');

promise_test(() => {
  let byobRequest;

  const stream = new ReadableStream({
    start(c) {
      const view = new Uint8Array(16);
      view[7] = 0x01;
      view[15] = 0x02;
      c.enqueue(view);
    },
    pull(controller) {
      byobRequest = controller.byobRequest;
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.read(new Uint8Array(8)).then(result => {
    assert_equals(result.done, false, 'done');

    const view = result.value;
    assert_equals(view.byteOffset, 0);
    assert_equals(view.byteLength, 8);
    assert_equals(view[7], 0x01);

    return reader.read(new Uint8Array(8));
  }).then(result => {
    assert_equals(result.done, false, 'done');

    assert_equals(byobRequest, undefined, 'byobRequest must be undefined');

    const view = result.value;
    assert_equals(view.byteOffset, 0);
    assert_equals(view.byteLength, 8);
    assert_equals(view[7], 0x02);
  });
}, 'ReadableStream with byte source: enqueue(), getReader(), then read(view) with a smaller views');

promise_test(() => {
  let controller;
  let viewInfo;

  const stream = new ReadableStream({
    start(c) {
      const view = new Uint8Array(1);
      view[0] = 0xff;
      c.enqueue(view);

      controller = c;
    },
    pull() {
      if (controller.byobRequest === undefined) {
        return;
      }

      const view = controller.byobRequest.view;
      viewInfo = extractViewInfo(view);

      view[0] = 0xaa;
      controller.byobRequest.respond(1);
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.read(new Uint16Array(1)).then(result => {
    assert_equals(result.done, false);

    const view = result.value;
    assert_equals(view.byteOffset, 0);
    assert_equals(view.byteLength, 2);
    assert_equals(view[0], 0xaaff);

    assert_equals(viewInfo.constructor, Uint8Array, 'view.constructor should be Uint8Array');
    assert_equals(viewInfo.bufferByteLength, 2, 'view.buffer.byteLength should be 2');
    assert_equals(viewInfo.byteOffset, 1, 'view.byteOffset should be 1');
    assert_equals(viewInfo.byteLength, 1, 'view.byteLength should be 1');
  });
}, 'ReadableStream with byte source: enqueue() 1 byte, getReader(), then read(view) with Uint16Array');

promise_test(() => {
  let pullCount = 0;

  let controller;
  let byobRequest;
  let viewInfo;
  let desiredSize;

  const stream = new ReadableStream({
    start(c) {
      const view = new Uint8Array(3);
      view[0] = 0x01;
      view[2] = 0x02;
      c.enqueue(view);

      controller = c;
    },
    pull() {
      byobRequest = controller.byobRequest;

      const view = controller.byobRequest.view;

      viewInfo = extractViewInfo(view);

      view[0] = 0x03;
      controller.byobRequest.respond(1);

      desiredSize = controller.desiredSize;

      ++pullCount;
    },
    type: 'bytes'
  });

  // Wait for completion of the start method to be reflected.
  return Promise.resolve().then(() => {
    const reader = stream.getReader({ mode: 'byob' });

    const promise = reader.read(new Uint16Array(2)).then(result => {
      assert_equals(result.done, false, 'done');

      const view = result.value;
      assert_equals(view.constructor, Uint16Array, 'constructor');
      assert_equals(view.buffer.byteLength, 4, 'buffer.byteLength');
      assert_equals(view.byteOffset, 0, 'byteOffset');
      assert_equals(view.byteLength, 2, 'byteLength');
      assert_equals(view[0], 0x0001, 'Contents are set');

      const p = reader.read(new Uint16Array(1));

      assert_equals(pullCount, 1);

      return p;
    }).then(result => {
      assert_equals(result.done, false, 'done');

      const view = result.value;
      assert_equals(view.buffer.byteLength, 2, 'buffer.byteLength');
      assert_equals(view.byteOffset, 0, 'byteOffset');
      assert_equals(view.byteLength, 2, 'byteLength');
      assert_equals(view[0], 0x0302, 'Contents are set');

      assert_not_equals(byobRequest, undefined, 'byobRequest must not be undefined');
      assert_equals(viewInfo.constructor, Uint8Array, 'view.constructor should be Uint8Array');
      assert_equals(viewInfo.bufferByteLength, 2, 'view.buffer.byteLength should be 2');
      assert_equals(viewInfo.byteOffset, 1, 'view.byteOffset should be 1');
      assert_equals(viewInfo.byteLength, 1, 'view.byteLength should be 1');
      assert_equals(desiredSize, 0, 'desiredSize should be zero');
    });

    assert_equals(pullCount, 0);

    return promise;
  });
}, 'ReadableStream with byte source: enqueue() 3 byte, getReader(), then read(view) with 2-element Uint16Array');

promise_test(t => {
  const stream = new ReadableStream({
    start(c) {
      const view = new Uint8Array(1);
      view[0] = 0xff;
      c.enqueue(view);
      c.close();
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });


  return promise_rejects(t, new TypeError(), reader.read(new Uint16Array(1)), 'read(view) must fail')
      .then(() => promise_rejects(t, new TypeError(), reader.closed, 'reader.closed should reject'));
}, 'ReadableStream with byte source: read(view) with Uint16Array on close()-d stream with 1 byte enqueue()-d must ' +
   'fail');

promise_test(t => {
  let controller;

  const stream = new ReadableStream({
    start(c) {
      const view = new Uint8Array(1);
      view[0] = 0xff;
      c.enqueue(view);

      controller = c;
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  const readPromise = reader.read(new Uint16Array(1));

  assert_throws(new TypeError(), () => controller.close(), 'controller.close() must throw');

  return promise_rejects(t, new TypeError(), readPromise, 'read(view) must fail')
      .then(() => promise_rejects(t, new TypeError(), reader.closed, 'reader.closed must reject'));
}, 'ReadableStream with byte source: A stream must be errored if close()-d before fulfilling read(view) with ' +
   'Uint16Array');

test(() => {
  let controller;

  new ReadableStream({
    start(c) {
      controller = c;
    },
    type: 'bytes'
  });

  // Enqueue a chunk so that the stream doesn't get closed. This is to check duplicate close() calls are rejected
  // even if the stream has not yet entered the closed state.
  const view = new Uint8Array(1);
  controller.enqueue(view);
  controller.close();

  assert_throws(new TypeError(), () => controller.close(), 'controller.close() must throw');
}, 'ReadableStream with byte source: Throw if close()-ed more than once');

test(() => {
  let controller;

  new ReadableStream({
    start(c) {
      controller = c;
    },
    type: 'bytes'
  });

  // Enqueue a chunk so that the stream doesn't get closed. This is to check enqueue() after close() is  rejected
  // even if the stream has not yet entered the closed state.
  const view = new Uint8Array(1);
  controller.enqueue(view);
  controller.close();

  assert_throws(new TypeError(), () => controller.enqueue(view), 'controller.close() must throw');
}, 'ReadableStream with byte source: Throw on enqueue() after close()');

promise_test(() => {
  let controller;
  let byobRequest;
  let viewInfo;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      byobRequest = controller.byobRequest;
      const view = controller.byobRequest.view;
      viewInfo = extractViewInfo(view);

      view[15] = 0x01;
      controller.byobRequest.respond(16);
      controller.close();
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.read(new Uint8Array(16)).then(result => {
    assert_equals(result.done, false);

    const view = result.value;
    assert_equals(view.byteOffset, 0);
    assert_equals(view.byteLength, 16);
    assert_equals(view[15], 0x01);

    return reader.read(new Uint8Array(16));
  }).then(result => {
    assert_equals(result.done, true);

    const view = result.value;
    assert_equals(view.byteOffset, 0);
    assert_equals(view.byteLength, 0);

    assert_not_equals(byobRequest, undefined, 'byobRequest must not be undefined');
    assert_equals(viewInfo.constructor, Uint8Array, 'view.constructor should be Uint8Array');
    assert_equals(viewInfo.bufferByteLength, 16, 'view.buffer.byteLength should be 16');
    assert_equals(viewInfo.byteOffset, 0, 'view.byteOffset should be 0');
    assert_equals(viewInfo.byteLength, 16, 'view.byteLength should be 16');
  });
}, 'ReadableStream with byte source: read(view), then respond() and close() in pull()');

promise_test(() => {
  let pullCount = 0;

  let controller;
  const viewInfos = [];

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      if (controller.byobRequest === undefined) {
        return;
      }

      for (let i = 0; i < 4; ++i) {
        const view = controller.byobRequest.view;
        viewInfos.push(extractViewInfo(view));

        view[0] = 0x01;
        controller.byobRequest.respond(1);
      }

      ++pullCount;
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return reader.read(new Uint32Array(1)).then(result => {
    assert_equals(result.done, false);

    const view = result.value;
    assert_equals(view.byteOffset, 0);
    assert_equals(view.byteLength, 4);
    assert_equals(view[0], 0x01010101);

    assert_equals(pullCount, 1, 'pull() should only be called once');

    for (let i = 0; i < 4; ++i) {
      assert_equals(viewInfos[i].constructor, Uint8Array, 'view.constructor should be Uint8Array');
      assert_equals(viewInfos[i].bufferByteLength, 4, 'view.buffer.byteLength should be 4');

      assert_equals(viewInfos[i].byteOffset, i, 'view.byteOffset should be i');
      assert_equals(viewInfos[i].byteLength, 4 - i, 'view.byteLength should be 4 - i');
    }
  });
}, 'ReadableStream with byte source: read(view) with Uint32Array, then fill it by multiple respond() calls');

promise_test(() => {
  let pullCount = 0;

  let controller;
  let byobRequest;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      byobRequest = controller.byobRequest;

      ++pullCount;
    },
    type: 'bytes'
  });

  const reader = stream.getReader();

  const p0 = reader.read().then(result => {
    assert_equals(pullCount, 1);

    controller.enqueue(new Uint8Array(2));

    // Since the queue has data no less than HWM, no more pull.
    assert_equals(pullCount, 1);

    assert_equals(result.done, false);

    const view = result.value;
    assert_equals(view.constructor, Uint8Array);
    assert_equals(view.buffer.byteLength, 1);
    assert_equals(view.byteOffset, 0);
    assert_equals(view.byteLength, 1);
  });

  assert_equals(pullCount, 0, 'No pull should have been made since the startPromise has not yet been handled');

  const p1 = reader.read().then(result => {
    assert_equals(pullCount, 1);

    assert_equals(result.done, false);

    const view = result.value;
    assert_equals(view.constructor, Uint8Array);
    assert_equals(view.buffer.byteLength, 2);
    assert_equals(view.byteOffset, 0);
    assert_equals(view.byteLength, 2);

    assert_equals(byobRequest, undefined, 'byobRequest must be undefined');
  });

  assert_equals(pullCount, 0, 'No pull should have been made since the startPromise has not yet been handled');

  controller.enqueue(new Uint8Array(1));

  assert_equals(pullCount, 0, 'No pull should have been made since the startPromise has not yet been handled');

  return Promise.all([p0, p1]);
}, 'ReadableStream with byte source: read() twice, then enqueue() twice');

promise_test(t => {
  let controller;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  const p0 = reader.read(new Uint8Array(16)).then(result => {
    assert_equals(result.done, true, '1st read: done');

    const view = result.value;
    assert_equals(view.buffer.byteLength, 16, '1st read: buffer.byteLength');
    assert_equals(view.byteOffset, 0, '1st read: byteOffset');
    assert_equals(view.byteLength, 0, '1st read: byteLength');
  });

  const p1 = reader.read(new Uint8Array(32)).then(result => {
    assert_equals(result.done, true, '2nd read: done');

    const view = result.value;
    assert_equals(view.buffer.byteLength, 32, '2nd read: buffer.byteLength');
    assert_equals(view.byteOffset, 0, '2nd read: byteOffset');
    assert_equals(view.byteLength, 0, '2nd read: byteLength');
  });

  controller.close();
  controller.byobRequest.respond(0);

  return Promise.all([p0, p1]);
}, 'ReadableStream with byte source: Multiple read(view), close() and respond()');

promise_test(t => {
  let controller;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  const p0 = reader.read(new Uint8Array(16)).then(result => {
    assert_equals(result.done, false, '1st read: done');

    const view = result.value;
    assert_equals(view.buffer.byteLength, 16, '1st read: buffer.byteLength');
    assert_equals(view.byteOffset, 0, '1st read: byteOffset');
    assert_equals(view.byteLength, 16, '1st read: byteLength');
  });

  const p1 = reader.read(new Uint8Array(16)).then(result => {
    assert_equals(result.done, false, '2nd read: done');

    const view = result.value;
    assert_equals(view.buffer.byteLength, 16, '2nd read: buffer.byteLength');
    assert_equals(view.byteOffset, 0, '2nd read: byteOffset');
    assert_equals(view.byteLength, 8, '2nd read: byteLength');
  });

  controller.enqueue(new Uint8Array(24));

  return Promise.all([p0, p1]);
}, 'ReadableStream with byte source: Multiple read(view), big enqueue()');

promise_test(t => {
  let controller;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  let bytesRead = 0;

  function pump() {
    return reader.read(new Uint8Array(7)).then(result => {
      if (result.done) {
        assert_equals(bytesRead, 1024);
        return undefined;
      }

      bytesRead += result.value.byteLength;

      return pump();
    });
  }
  const promise = pump();

  controller.enqueue(new Uint8Array(512));
  controller.enqueue(new Uint8Array(512));
  controller.close();

  return promise;
}, 'ReadableStream with byte source: Multiple read(view) and multiple enqueue()');

promise_test(t => {
  let byobRequest;
  const stream = new ReadableStream({
    pull(controller) {
      byobRequest = controller.byobRequest;
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return promise_rejects(t, new TypeError(), reader.read(), 'read() must fail')
      .then(() => assert_equals(byobRequest, undefined, 'byobRequest must be undefined'));
}, 'ReadableStream with byte source: read(view) with passing undefined as view must fail');

promise_test(t => {
  let byobRequest;
  const stream = new ReadableStream({
    pull(controller) {
      byobRequest = controller.byobRequest;
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return promise_rejects(t, new TypeError(), reader.read(new Uint8Array(0)), 'read(view) must fail')
      .then(() => assert_equals(byobRequest, undefined, 'byobRequest must be undefined'));
}, 'ReadableStream with byte source: read(view) with zero-length view must fail');

promise_test(t => {
  const stream = new ReadableStream({
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return promise_rejects(t, new TypeError(), reader.read({}), 'read(view) must fail');
}, 'ReadableStream with byte source: read(view) with passing an empty object as view must fail');

promise_test(t => {
  const stream = new ReadableStream({
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return promise_rejects(t, new TypeError(),
                         reader.read({ buffer: new ArrayBuffer(10), byteOffset: 0, byteLength: 10 }),
                         'read(view) must fail');
}, 'ReadableStream with byte source: Even read(view) with passing ArrayBufferView like object as view must fail');

promise_test(t => {
  const stream = new ReadableStream({
    start(c) {
      c.error(error1);
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader();

  return promise_rejects(t, error1, reader.read(), 'read() must fail');
}, 'ReadableStream with byte source: read() on an errored stream');

promise_test(t => {
  let controller;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    type: 'bytes'
  });

  const reader = stream.getReader();

  const promise = promise_rejects(t, error1, reader.read(), 'read() must fail');

  controller.error(error1);

  return promise;
}, 'ReadableStream with byte source: read(), then error()');

promise_test(t => {
  const stream = new ReadableStream({
    start(c) {
      c.error(error1);
    },
    pull: t.unreached_func('pull() should not be called'),
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return promise_rejects(t, error1, reader.read(new Uint8Array(1)), 'read() must fail');
}, 'ReadableStream with byte source: read(view) on an errored stream');

promise_test(t => {
  let controller;

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  const promise = promise_rejects(t, error1, reader.read(new Uint8Array(1)), 'read() must fail');

  controller.error(error1);

  return promise;
}, 'ReadableStream with byte source: read(view), then error()');

promise_test(t => {
  let controller;
  let byobRequest;

  const testError = new TypeError('foo');

  const stream = new ReadableStream({
    start(c) {
      controller = c;
    },
    pull() {
      byobRequest = controller.byobRequest;
      throw testError;
    },
    type: 'bytes'
  });

  const reader = stream.getReader();

  const promise = promise_rejects(t, testError, reader.read(), 'read() must fail');
  return promise_rejects(t, testError, promise.then(() => reader.closed))
      .then(() => assert_equals(byobRequest, undefined, 'byobRequest must be undefined'));
}, 'ReadableStream with byte source: Throwing in pull function must error the stream');

promise_test(t => {
  let byobRequest;

  const stream = new ReadableStream({
    pull(controller) {
      byobRequest = controller.byobRequest;
      controller.error(error1);
      throw new TypeError('foo');
    },
    type: 'bytes'
  });

  const reader = stream.getReader();

  return promise_rejects(t, error1, reader.read(), 'read() must fail')
      .then(() => promise_rejects(t, error1, reader.closed, 'closed must fail'))
      .then(() => assert_equals(byobRequest, undefined, 'byobRequest must be undefined'));
}, 'ReadableStream with byte source: Throwing in pull in response to read() must be ignored if the stream is ' +
   'errored in it');

promise_test(t => {
  let byobRequest;

  const testError = new TypeError('foo');

  const stream = new ReadableStream({
    pull(controller) {
      byobRequest = controller.byobRequest;
      throw testError;
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return promise_rejects(t, testError, reader.read(new Uint8Array(1)), 'read(view) must fail')
      .then(() => promise_rejects(t, testError, reader.closed, 'reader.closed must reject'))
      .then(() => assert_not_equals(byobRequest, undefined, 'byobRequest must not be undefined'));
}, 'ReadableStream with byte source: Throwing in pull in response to read(view) function must error the stream');

promise_test(t => {
  let byobRequest;

  const stream = new ReadableStream({
    pull(controller) {
      byobRequest = controller.byobRequest;
      controller.error(error1);
      throw new TypeError('foo');
    },
    type: 'bytes'
  });

  const reader = stream.getReader({ mode: 'byob' });

  return promise_rejects(t, error1, reader.read(new Uint8Array(1)), 'read(view) must fail')
      .then(() => promise_rejects(t, error1, reader.closed, 'closed must fail'))
      .then(() => assert_not_equals(byobRequest, undefined, 'byobRequest must not be undefined'));
}, 'ReadableStream with byte source: Throwing in pull in response to read(view) must be ignored if the stream is ' +
   'errored in it');

promise_test(() => {
  let byobRequest;
  const rs = new ReadableStream({
    pull(controller) {
      byobRequest = controller.byobRequest;
      byobRequest.respond(4);
    },
    type: 'bytes'
  });
  const reader = rs.getReader({ mode: 'byob' });
  const view = new Uint8Array(16);
  return reader.read(view).then(() => {
    assert_throws(new TypeError(), () => byobRequest.respond(4), 'respond() should throw a TypeError');
  });
}, 'calling respond() twice on the same byobRequest should throw');

promise_test(() => {
  let byobRequest;
  const newView = () => new Uint8Array(16);
  const rs = new ReadableStream({
    pull(controller) {
      byobRequest = controller.byobRequest;
      byobRequest.respondWithNewView(newView());
    },
    type: 'bytes'
  });
  const reader = rs.getReader({ mode: 'byob' });
  return reader.read(newView()).then(() => {
    assert_throws(new TypeError(), () => byobRequest.respondWithNewView(newView()),
                  'respondWithNewView() should throw a TypeError');
  });
}, 'calling respondWithNewView() twice on the same byobRequest should throw');

promise_test(() => {
  let byobRequest;
  let resolvePullCalledPromise;
  const pullCalledPromise = new Promise(resolve => {
    resolvePullCalledPromise = resolve;
  });
  let resolvePull;
  const rs = new ReadableStream({
    pull(controller) {
      byobRequest = controller.byobRequest;
      resolvePullCalledPromise();
      return new Promise(resolve => {
        resolvePull = resolve;
      });
    },
    type: 'bytes'
  });
  const reader = rs.getReader({ mode: 'byob' });
  const readPromise = reader.read(new Uint8Array(16));
  return pullCalledPromise.then(() => {
    const cancelPromise = reader.cancel('meh');
    resolvePull();
    byobRequest.respond(0);
    return Promise.all([readPromise, cancelPromise]).then(() => {
      assert_throws(new TypeError(), () => byobRequest.respond(0), 'respond() should throw');
    });
  });
}, 'calling respond(0) twice on the same byobRequest should throw even when closed');

promise_test(() => {
  let resolvePullCalledPromise;
  const pullCalledPromise = new Promise(resolve => {
    resolvePullCalledPromise = resolve;
  });
  let resolvePull;
  const rs = new ReadableStream({
    pull() {
      resolvePullCalledPromise();
      return new Promise(resolve => {
        resolvePull = resolve;
      });
    },
    type: 'bytes'
  });
  const reader = rs.getReader({ mode: 'byob' });
  reader.read(new Uint8Array(16));
  return pullCalledPromise.then(() => {
    resolvePull();
    return delay(0).then(() => {
      assert_throws(new TypeError(), () => reader.releaseLock(), 'releaseLock() should throw');
    });
  });
}, 'pull() resolving should not make releaseLock() possible');

promise_test(() => {
  // Tests https://github.com/whatwg/streams/issues/686

  let controller;
  const rs = new ReadableStream({
    autoAllocateChunkSize: 128,
    start(c) {
      controller = c;
    },
    type: 'bytes'
  });

  const readPromise = rs.getReader().read();

  const br = controller.byobRequest;
  controller.close();

  br.respond(0);

  return readPromise;
}, 'ReadableStream with byte source: default reader + autoAllocateChunkSize + byobRequest interaction');

test(() => {
  const ReadableStreamBYOBReader = new ReadableStream({ type: 'bytes' }).getReader({ mode: 'byob' }).constructor;
  const stream = new ReadableStream({ type: 'bytes' });
  new ReadableStreamBYOBReader(stream);
}, 'ReadableStreamBYOBReader can be constructed directly');

test(() => {
  const ReadableStreamBYOBReader = new ReadableStream({ type: 'bytes' }).getReader({ mode: 'byob' }).constructor;
  assert_throws(new TypeError(), () => new ReadableStreamBYOBReader({}), 'constructor must throw');
}, 'ReadableStreamBYOBReader constructor requires a ReadableStream argument');

test(() => {
  const ReadableStreamBYOBReader = new ReadableStream({ type: 'bytes' }).getReader({ mode: 'byob' }).constructor;
  const stream = new ReadableStream({ type: 'bytes' });
  stream.getReader();
  assert_throws(new TypeError(), () => new ReadableStreamBYOBReader(stream), 'constructor must throw');
}, 'ReadableStreamBYOBReader constructor requires an unlocked ReadableStream');

test(() => {
  const ReadableStreamBYOBReader = new ReadableStream({ type: 'bytes' }).getReader({ mode: 'byob' }).constructor;
  const stream = new ReadableStream();
  assert_throws(new TypeError(), () => new ReadableStreamBYOBReader(stream), 'constructor must throw');
}, 'ReadableStreamBYOBReader constructor requires a ReadableStream with type "bytes"');

test(() => {
  assert_throws(new RangeError(), () => new ReadableStream({ type: 'bytes' }, {
    size() {
      return 1;
    }
  }), 'constructor should throw for size function');

  assert_throws(new RangeError(), () => new ReadableStream({ type: 'bytes' }, { size: null }),
                'constructor should throw for size defined');

  assert_throws(new RangeError(),
                () => new ReadableStream({ type: 'bytes' }, new CountQueuingStrategy({ highWaterMark: 1 })),
                'constructor should throw when strategy is CountQueuingStrategy');

  assert_throws(new RangeError(),
                () => new ReadableStream({ type: 'bytes' }, new ByteLengthQueuingStrategy({ highWaterMark: 512 })),
                'constructor should throw when strategy is ByteLengthQueuingStrategy');

  class HasSizeMethod {
    size() {}
 }

  assert_throws(new RangeError(), () => new ReadableStream({ type: 'bytes' }, new HasSizeMethod()),
                'constructor should throw when size on the prototype chain');
}, 'ReadableStream constructor should not accept a strategy with a size defined if type is "bytes"');

done();
