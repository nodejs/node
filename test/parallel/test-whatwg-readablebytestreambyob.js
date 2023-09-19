'use strict';

const common = require('../common');

const {
  open,
} = require('fs/promises');

const {
  Buffer,
} = require('buffer');

class Source {
  async start(controller) {
    this.file = await open(__filename);
    this.controller = controller;
  }

  async pull(controller) {
    const byobRequest = controller.byobRequest;
    const view = byobRequest.view;

    const {
      bytesRead,
    } = await this.file.read({
      buffer: view,
      offset: view.byteOffset,
      length: view.byteLength
    });

    if (bytesRead === 0) {
      await this.file.close();
      this.controller.close();
    }

    byobRequest.respond(bytesRead);
  }

  get type() { return 'bytes'; }

  get autoAllocateChunkSize() { return 1024; }
}

(async () => {
  const source = new Source();
  const stream = new ReadableStream(source);

  const { emitWarning } = process;

  process.emitWarning = common.mustNotCall();

  try {
    const reader = stream.getReader({ mode: 'byob' });

    let result;
    do {
      result = await reader.read(Buffer.alloc(100));
    } while (!result.done);
  } finally {
    process.emitWarning = emitWarning;
  }
})().then(common.mustCall());
