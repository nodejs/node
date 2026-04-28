'use strict';
require('../common');
const { Readable } = require('stream');

{
  const r = Readable.from([]);
  // Cancelling reader while closing should not cause uncaught exceptions
  r.on('close', () => reader.cancel());

  const reader = Readable.toWeb(r).getReader();
  reader.read();
}

// Cancelling a web ReadableStream while the underlying Readable is actively
// producing data should not throw ERR_INVALID_STATE. The onData handler in
// newReadableStreamFromStreamReadable must check wasCanceled before calling
// controller.enqueue(). See: https://github.com/nodejs/node/issues/54205
{
  const readable = new Readable({
    read() {
      this.push(Buffer.alloc(1024));
    },
  });

  const webStream = Readable.toWeb(readable);
  const reader = webStream.getReader();

  (async () => {
    await reader.read();
    await reader.read();
    reader.releaseLock();
    await webStream.cancel();
  })();
}
