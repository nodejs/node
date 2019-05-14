// META: global=worker

'use strict';

const badChunks = [
  {
    name: 'undefined',
    value: undefined
  },
  {
    name: 'null',
    value: null
  },
  {
    name: 'numeric',
    value: 3.14
  },
  {
    name: 'object, not BufferSource',
    value: {}
  },
  {
    name: 'array',
    value: [65]
  },
  {
    name: 'detached ArrayBufferView',
    value: (() => {
      const u8 = new Uint8Array([65]);
      const ab = u8.buffer;
      const mc = new MessageChannel();
      mc.port1.postMessage(ab, [ab]);
      return u8;
    })()
  },
  {
    name: 'detached ArrayBuffer',
    value: (() => {
      const u8 = new Uint8Array([65]);
      const ab = u8.buffer;
      const mc = new MessageChannel();
      mc.port1.postMessage(ab, [ab]);
      return ab;
    })()
  },
  {
    name: 'SharedArrayBuffer',
    // Use a getter to postpone construction so that all tests don't fail where
    // SharedArrayBuffer is not yet implemented.
    get value() {
      return new SharedArrayBuffer();
    }
  },
  {
    name: 'shared Uint8Array',
    get value() {
      new Uint8Array(new SharedArrayBuffer())
    }
  }
];

for (const chunk of badChunks) {
  promise_test(async t => {
    const tds = new TextDecoderStream();
    const reader = tds.readable.getReader();
    const writer = tds.writable.getWriter();
    const writePromise = writer.write(chunk.value);
    const readPromise = reader.read();
    await promise_rejects(t, new TypeError(), writePromise, 'write should reject');
    await promise_rejects(t, new TypeError(), readPromise, 'read should reject');
  }, `chunk of type ${chunk.name} should error the stream`);
}
