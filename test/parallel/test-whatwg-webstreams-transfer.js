// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');

const {
  ReadableStream,
  WritableStream,
  TransformStream,
} = require('stream/web');

const {
  Worker
} = require('worker_threads');

const {
  isReadableStream,
} = require('internal/webstreams/readablestream');

const {
  isWritableStream,
} = require('internal/webstreams/writablestream');

const {
  isTransformStream,
} = require('internal/webstreams/transformstream');

const {
  makeTransferable,
  kClone,
  kTransfer,
  kDeserialize,
} = require('internal/worker/js_transferable');

const assert = require('assert');

const theData = 'hello';

{
  const { port1, port2 } = new MessageChannel();
  port1.onmessageerror = common.mustNotCall();
  port2.onmessageerror = common.mustNotCall();

  // This test takes the ReadableStream and transfers it to the
  // port1 first, then again to port2, which reads the data.
  // Internally, this sets up a pipelined data flow that is
  // important to understand in case this test fails..
  //
  // Specifically:
  //
  // 1. We start with ReadableStream R1,
  // 2. Calling port2.postMessage causes a new internal WritableStream W1
  //    and a new ReadableStream R2 to be created, both of which are coupled
  //    to each other via a pair of MessagePorts P1 and P2.
  // 3. ReadableStream R2 is passed to the port1.onmessage callback as the
  //    data property of the MessageEvent, and R1 is configured to pipeTo W1.
  // 4. Within port1.onmessage, we transfer ReadableStream R2 to port1, which
  //    creates a new internal WritableStream W2 and a new ReadableStream R3,
  //    both of which are coupled to each other via a pair of MessagePorts
  //    P3 and P4.
  // 5. ReadableStream R3 is passed to the port2.onmessage callback as the
  //    data property of the MessageEvent, and R2 is configured to pipeTo W2.
  // 6. Once the reader is attached to R3 in the port2.onmessage callback,
  //    a message is sent along the path: R3 -> P4 -> P3 -> R2 -> P2 -> P1 -> R1
  //    to begin pulling the data. The data is then pushed along the pipeline
  //    R1 -> W1 -> P1 -> P2 -> R2 -> W2 -> P3 -> P4 -> R3
  // 7. The MessagePorts P1, P2, P3, and P4 serve as a control channel for
  //    passing data and control instructions, potentially across realms,
  //    to the other ReadableStream and WritableStream instances.
  //
  // If this test experiences timeouts (hangs without finishing), it's most
  // likely because the control instructions are somehow broken and the
  // MessagePorts are not being closed properly or it could be caused by
  // failing the close R1's controller which signals the end of the data
  // flow.

  const readable = new ReadableStream({
    start: common.mustCall((controller) => {
      controller.enqueue(theData);
      controller.close();
    }),
  });

  port2.onmessage = common.mustCall(({ data }) => {
    assert(isReadableStream(data));

    const reader = data.getReader();
    reader.read().then(common.mustCall((chunk) => {
      assert.deepStrictEqual(chunk, { done: false, value: theData });
    }));

    port2.close();
  });

  port1.onmessage = common.mustCall(({ data }) => {
    assert(isReadableStream(data));
    assert(!data.locked);
    port1.postMessage(data, [data]);
    assert(data.locked);
  });

  assert.throws(() => port2.postMessage(readable), {
    code: 'ERR_MISSING_TRANSFERABLE_IN_TRANSFER_LIST',
  });

  port2.postMessage(readable, [readable]);
  assert(readable.locked);
}

{
  const { port1, port2 } = new MessageChannel();
  port1.onmessageerror = common.mustNotCall();
  port2.onmessageerror = common.mustNotCall();

  // Like the ReadableStream test above, this sets up a pipeline
  // through which the data flows...
  //
  // We start with WritableStream W1, which is transfered to port1.
  // Doing so creates an internal ReadableStream R1 and WritableStream W2,
  // which are coupled together with MessagePorts P1 and P2.
  // The port1.onmessage callback receives WritableStream W2 and
  // immediately transfers that to port2. Doing so creates an internal
  // ReadableStream R2 and WritableStream W3, which are coupled together
  // with MessagePorts P3 and P4. WritableStream W3 is handed off to
  // port2.onmessage.
  //
  // When the writer on port2.onmessage writes the chunk of data, it
  // gets passed along the pipeline:
  // W3 -> P4 -> P3 -> R2 -> W2 -> P2 -> P1 -> R1 -> W1

  const writable = new WritableStream({
    write: common.mustCall((chunk) => {
      assert.strictEqual(chunk, theData);
    }),
  });

  port2.onmessage = common.mustCall(({ data }) => {
    assert(isWritableStream(data));
    assert(!data.locked);
    const writer = data.getWriter();
    writer.write(theData).then(common.mustCall());
    writer.close();
    port2.close();
  });

  port1.onmessage = common.mustCall(({ data }) => {
    assert(isWritableStream(data));
    assert(!data.locked);
    port1.postMessage(data, [data]);
    assert(data.locked);
  });

  assert.throws(() => port2.postMessage(writable), {
    code: 'ERR_MISSING_TRANSFERABLE_IN_TRANSFER_LIST',
  });

  port2.postMessage(writable, [writable]);
  assert(writable.locked);
}

{
  const { port1, port2 } = new MessageChannel();
  port1.onmessageerror = common.mustNotCall();
  port2.onmessageerror = common.mustNotCall();

  // The data flow here is actually quite complicated, and is a combination
  // of the WritableStream and ReadableStream examples above.
  //
  // We start with TransformStream T1, which creates ReadableStream R1,
  // and WritableStream W1.
  //
  // When T1 is transfered to port1.onmessage, R1 and W1 are individually
  // transfered.
  //
  // When R1 is transfered, it creates internal WritableStream W2, and
  // new ReadableStream R2, coupled together via MessagePorts P1 and P2.
  //
  // When W1 is transfered, it creates internal ReadableStream R3 and
  // new WritableStream W3, coupled together via MessagePorts P3 and P4.
  //
  // A new TransformStream T2 is created that owns ReadableStream R2 and
  // WritableStream W3. The port1.onmessage callback immediately transfers
  // that to port2.onmessage.
  //
  // When T2 is transfered, R2 and W3 are individually transfered.
  //
  // When R2 is transfered, it creates internal WritableStream W4, and
  // ReadableStream R4, coupled together via MessagePorts P5 and P6.
  //
  // When W3 is transfered, it creates internal ReadableStream R5, and
  // WritableStream W5, coupled together via MessagePorts P7 and P8.
  //
  // A new TransformStream T3 is created that owns ReadableStream R4 and
  // WritableStream W5.
  //
  // port1.onmessage then writes a chunk of data. That chunk of data
  // flows through the pipeline to T1:
  //
  // W5 -> P8 -> P7 -> R5 -> W3 -> P4 -> P3 -> R3 -> W1 -> T1
  //
  // T1 performs the transformation, then pushes the chunk back out
  // along the pipeline:
  //
  // T1 -> R1 -> W2 -> P1 -> P2 -> R2 -> W4 -> P5 -> P6 -> R4

  const transform = new TransformStream({
    transform(chunk, controller) {
      controller.enqueue(chunk.toUpperCase());
    }
  });

  port2.onmessage = common.mustCall(({ data }) => {
    assert(isTransformStream(data));
    const writer = data.writable.getWriter();
    const reader = data.readable.getReader();
    Promise.all([
      writer.write(theData),
      writer.close(),
      reader.read().then(common.mustCall((result) => {
        assert(!result.done);
        assert.strictEqual(result.value, theData.toUpperCase());
      })),
      reader.read().then(common.mustCall((result) => {
        assert(result.done);
      })),
    ]).then(common.mustCall());
    port2.close();
  });

  port1.onmessage = common.mustCall(({ data }) => {
    assert(isTransformStream(data));
    assert(!data.readable.locked);
    assert(!data.writable.locked);
    port1.postMessage(data, [data]);
    assert(data.readable.locked);
    assert(data.writable.locked);
  });

  assert.throws(() => port2.postMessage(transform), {
    code: 'ERR_MISSING_TRANSFERABLE_IN_TRANSFER_LIST',
  });

  port2.postMessage(transform, [transform]);
  assert(transform.readable.locked);
  assert(transform.writable.locked);
}

{
  const { port1, port2 } = new MessageChannel();
  let controller;

  const readable = new ReadableStream({
    start(c) { controller = c; },

    cancel: common.mustCall((error) => {
      assert.strictEqual(error.code, 25);  // DataCloneError
    }),
  });

  port1.onmessage = ({ data }) => {
    const reader = data.getReader();
    assert.rejects(reader.read(), {
      code: 25,  // DataCloneError
    });
    port1.close();
  };

  port2.postMessage(readable, [readable]);

  const notActuallyTransferable = makeTransferable({
    [kClone]() {
      return {
        data: {},
        deserializeInfo: 'nothing that will work',
      };
    },
    [kDeserialize]: common.mustNotCall(),
  });

  controller.enqueue(notActuallyTransferable);
}

{
  const { port1, port2 } = new MessageChannel();

  const source = {
    abort: common.mustCall((error) => {
      process.nextTick(() => {
        assert.strictEqual(error.code, 25);
        assert.strictEqual(error.name, 'DataCloneError');
      });
    })
  };

  const writable = new WritableStream(source);

  const notActuallyTransferable = makeTransferable({
    [kClone]() {
      return {
        data: {},
        deserializeInfo: 'nothing that will work',
      };
    },
    [kDeserialize]: common.mustNotCall(),
  });

  port1.onmessage = common.mustCall(({ data }) => {
    const writer = data.getWriter();

    assert.rejects(writer.closed, {
      code: 25,
      name: 'DataCloneError',
    });

    writer.write(notActuallyTransferable).then(common.mustCall());

    port1.close();
  });

  port2.postMessage(writable, [writable]);
}

{
  const error = new Error('boom');
  const { port1, port2 } = new MessageChannel();

  const source = {
    abort: common.mustCall((reason) => {
      process.nextTick(() => {
        assert.deepStrictEqual(reason, error);

        // Reason is a clone of the original error.
        assert.notStrictEqual(reason, error);
      });
    }),
  };

  const writable = new WritableStream(source);

  port1.onmessage = common.mustCall(({ data }) => {
    const writer = data.getWriter();

    assert.rejects(writer.closed, error);

    writer.abort(error).then(common.mustCall());
    port1.close();
  });

  port2.postMessage(writable, [writable]);
}

{
  const { port1, port2 } = new MessageChannel();

  const source = {
    abort: common.mustCall((error) => {
      process.nextTick(() => assert.strictEqual(error.code, 25));
    })
  };

  const writable = new WritableStream(source);

  port1.onmessage = common.mustCall(({ data }) => {
    const writer = data.getWriter();

    const m = new WebAssembly.Memory({ initial: 1 });

    assert.rejects(writer.abort(m), {
      code: 25
    });
    port1.close();
  });

  port2.postMessage(writable, [writable]);
}

{
  // Verify that the communication works across worker threads...

  const worker = new Worker(`
    const {
      isReadableStream,
    } = require('internal/webstreams/readablestream');

    const {
      parentPort,
    } = require('worker_threads');

    const assert = require('assert');

    const tracker = new assert.CallTracker();
    process.on('exit', () => {
      tracker.verify();
    });

    parentPort.onmessage = tracker.calls(({ data }) => {
      assert(isReadableStream(data));
      const reader = data.getReader();
      reader.read().then(tracker.calls((result) => {
        assert(!result.done);
        assert(result.value instanceof Uint8Array);
      }));
      parentPort.close();
    });
    parentPort.onmessageerror = () => assert.fail('should not be called');
  `, { eval: true });

  worker.on('error', common.mustNotCall());

  const readable = new ReadableStream({
    start(controller) {
      controller.enqueue(new Uint8Array(10));
      controller.close();
    }
  });

  worker.postMessage(readable, [readable]);
}

{
  const source = {
    cancel: common.mustCall(),
  };

  const readable = new ReadableStream(source);

  const { port1, port2 } = new MessageChannel();

  port1.onmessage = common.mustCall(({ data }) => {
    data.cancel().then(common.mustCall());
    port1.close();
  });

  port2.postMessage(readable, [readable]);
}

{
  const source = {
    cancel: common.mustCall((error) => {
      process.nextTick(() => assert(error.code, 25));
    }),
  };

  const readable = new ReadableStream(source);

  const { port1, port2 } = new MessageChannel();

  port1.onmessage = common.mustCall(({ data }) => {
    const m = new WebAssembly.Memory({ initial: 1 });

    const reader = data.getReader();

    const cancel = reader.cancel(m);

    reader.closed.then(common.mustCall());

    assert.rejects(cancel, {
      code: 25
    });

    port1.close();
  });

  port2.postMessage(readable, [readable]);
}

{
  const source = {
    abort: common.mustCall((error) => {
      process.nextTick(() => {
        assert.strictEqual(error.code, 25);
      });
    }),
  };

  const writable = new WritableStream(source);

  const { port1, port2 } = new MessageChannel();

  port1.onmessage = common.mustCall(({ data }) => {
    const m = new WebAssembly.Memory({ initial: 1 });
    const writer = data.getWriter();
    const write = writer.write(m);
    assert.rejects(write, { code: 25 });
    port1.close();
  });

  port2.postMessage(writable, [writable]);
}

{
  const readable = new ReadableStream();
  readable.getReader();
  assert.throws(() => readable[kTransfer](), {
    code: 25
  });

  const writable = new WritableStream();
  writable.getWriter();
  assert.throws(() => writable[kTransfer](), {
    code: 25
  });
}
