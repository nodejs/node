'use strict';

const common = require('../common');
const assert = require('node:assert');

let pass = 0;

{
  // ReadableStream with byte source: respondWithNewView() throws if the
  // supplied view's buffer has a different length (in the closed state)
  const stream = new ReadableStream({
    pull: common.mustCall(async (c) => {
      const view = new Uint8Array(new ArrayBuffer(10), 0, 0);

      c.close();

      assert.throws(() => c.byobRequest.respondWithNewView(view), {
        code: 'ERR_INVALID_ARG_VALUE',
        name: 'RangeError',
      });
      pass++;
    }),
    type: 'bytes',
  });

  const reader = stream.getReader({ mode: 'byob' });
  reader.read(new Uint8Array([4, 5, 6]));
}

{
  // ReadableStream with byte source: respondWithNewView() throws if the
  // supplied view's buffer has been detached (in the closed state)
  const stream = new ReadableStream({
    pull: common.mustCall((c) => {
      c.close();

      // Detach it by reading into it
      const view = new Uint8Array([1, 2, 3]);
      reader.read(view);

      assert.throws(() => c.byobRequest.respondWithNewView(view), {
        code: 'ERR_INVALID_STATE',
        name: 'TypeError',
      });
      pass++;
    }),
    type: 'bytes',
  });

  const reader = stream.getReader({ mode: 'byob' });
  reader.read(new Uint8Array([4, 5, 6]));
}

{
  const stream = new ReadableStream({
    start(c) {
      c.enqueue(new Uint8Array([1, 2, 3]));
    },
    type: 'bytes',
  });
  const reader = stream.getReader({ mode: 'byob' });
  const view = new Uint8Array();
  assert
    .rejects(reader.read(view), {
      code: 'ERR_INVALID_STATE',
      name: 'TypeError',
    })
    .then(common.mustCall());
}

{
  const stream = new ReadableStream({
    start(c) {
      c.enqueue(new Uint8Array([1, 2, 3]));
    },
    type: 'bytes',
  });
  const reader = stream.getReader({ mode: 'byob' });
  const view = new Uint8Array(new ArrayBuffer(10), 0, 0);
  assert
    .rejects(reader.read(view), {
      code: 'ERR_INVALID_STATE',
      name: 'TypeError',
    })
    .then(common.mustCall());
}

process.on('exit', () => assert.strictEqual(pass, 2));
