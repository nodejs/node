'use strict';

require('../common');
const assert = require('assert');

{
  // https://github.com/nodejs/node/issues/46296
  const stream = new ReadableStream({
    start(controller) {
      controller.close();
    },
    type: 'bytes',
  });

  const clone = structuredClone(stream, { transfer: [stream] });

  assert(clone instanceof ReadableStream);
}
