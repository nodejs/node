// META: global=window,worker
// META: script=/common/gc.js
'use strict';

// See https://crbug.com/390646657 for details.
promise_test(async () => {
  const written = new WritableStream({
    write(chunk) {
      return new Promise(resolve => {});
    }
  }).getWriter().write('just nod if you can hear me');
  for (let i = 0; i < 5; ++i)
    await garbageCollect();
}, 'Garbage-collecting a stream writer with a pending write should not crash');

promise_test(async () => {
  const closed = new WritableStream({
    write(chunk) { }
  }).getWriter().closed;
  for (let i = 0; i < 5; ++i)
    await garbageCollect();
}, 'Garbage-collecting a stream writer should not crash with closed promise is retained');

promise_test(async () => {
  let writer = new WritableStream({
    write(chunk) { return new Promise(resolve => {}); },
    close() { return new Promise(resolve => {}); }
  }).getWriter();
  writer.write('is there anyone home?');
  writer.close();
  writer = null;
  for (let i = 0; i < 5; ++i)
    await garbageCollect();
}, 'Garbage-collecting a stream writer should not crash with close promise pending');

promise_test(async () => {
  const ready = new WritableStream({
    write(chunk) { }
  }, {highWaterMark: 0}).getWriter().ready;
  for (let i = 0; i < 5; ++i)
    await garbageCollect();
}, 'Garbage-collecting a stream writer should not crash when backpressure is being applied');

// Repro for https://crbug.com/455800266
promise_test(async () => {
  // This logic is wrapped in a function to make it easy to garbage collect all
  // references to the WritableStream.
  const createWritableStream = async () => {
    const WRITE_COUNT = 2;
    let writes_done = 0;
    const { promise, resolve } = Promise.withResolvers();

    const ws = new WritableStream({
      write() {
        if (writes_done === WRITE_COUNT) {
          // Will never resolve, leaving the write operation pending.
          return new Promise(resolve => { });
        }
        ++writes_done;
        return promise;
      }
    });

    const writer = ws.getWriter();
    await writer.ready;

    const writeChunks = () => {
      for (let i = 0; i < WRITE_COUNT; ++i) {
        const ready = writer.ready;
        writer.write("chunk");
      }
    };

    // Apply backpressure.
    writeChunks();

    // Release backpressure.
    resolve();
    await writer.ready;

    // Apply backpressure again.
    writeChunks();
  };

  await createWritableStream();

  for (let i = 0; i < 5; ++i) {
    await garbageCollect();
  }
}, "WritableStream should not crash when garbage collected with backpressure");
