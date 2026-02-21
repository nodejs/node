import { mustCall } from '../common/index.mjs';
import { ReadableStream } from 'stream/web';
import assert from 'assert';

{
  // Test tee() with close in the nextTick after enqueue
  async function read(stream) {
    const chunks = [];
    for await (const chunk of stream)
      chunks.push(chunk);
    return Buffer.concat(chunks).toString();
  }

  const [r1, r2] = new ReadableStream({
    start(controller) {
      process.nextTick(() => {
        controller.enqueue(new Uint8Array([102, 111, 111, 98, 97, 114]));

        process.nextTick(() => {
          controller.close();
        });
      });
    }
  }).tee();

  (async () => {
    const [dataReader1, dataReader2] = await Promise.all([
      read(r1),
      read(r2),
    ]);

    assert.strictEqual(dataReader1, dataReader2);
    assert.strictEqual(dataReader1, 'foobar');
    assert.strictEqual(dataReader2, 'foobar');
  })().then(mustCall());
}

{
  // Test ReadableByteStream.tee() with close in the nextTick after enqueue
  async function read(stream) {
    const chunks = [];
    for await (const chunk of stream)
      chunks.push(chunk);
    return Buffer.concat(chunks).toString();
  }

  const [r1, r2] = new ReadableStream({
    type: 'bytes',
    start(controller) {
      process.nextTick(() => {
        controller.enqueue(new Uint8Array([102, 111, 111, 98, 97, 114]));

        process.nextTick(() => {
          controller.close();
        });
      });
    }
  }).tee();

  (async () => {
    const [dataReader1, dataReader2] = await Promise.all([
      read(r1),
      read(r2),
    ]);

    assert.strictEqual(dataReader1, dataReader2);
    assert.strictEqual(dataReader1, 'foobar');
    assert.strictEqual(dataReader2, 'foobar');
  })().then(mustCall());
}
