// Flags: --experimental-worker
'use strict';
require('../common');

const { MessageChannel } = require('worker_threads');

{
  const { port1, port2 } = new MessageChannel();

  // Throwing in the getter should not crash.
  Object.defineProperty(port1, 'onmessage', {
    get() {
      throw new Error('eyecatcher');
    }
  });

  port2.postMessage({ foo: 'bar' });
}

{
  const { port1, port2 } = new MessageChannel();

  // Returning a non-function in the getter should not crash.
  Object.defineProperty(port1, 'onmessage', {
    get() {
      return 42;
    }
  });

  port2.postMessage({ foo: 'bar' });
}
