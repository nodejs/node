'use strict';
const common = require('../common');
const assert = require('assert');

const { MessageChannel } = require('worker_threads');

{
  const { port1, port2 } = new MessageChannel();

  // Returning a non-function in the getter should not crash.
  Object.defineProperty(port1, 'onmessage', {
    get() {
      port1.unref();
      return 42;
    }
  });

  port2.postMessage({ foo: 'bar' });

  // We need to start the port manually because .onmessage assignment tracking
  // has been overridden.
  port1.start();
  port1.ref();
}

{
  const err = new Error('eyecatcher');
  process.on('uncaughtException', common.mustCall((exception) => {
    port1.unref();
    assert.strictEqual(exception, err);
  }));

  const { port1, port2 } = new MessageChannel();

  // Throwing in the getter should not crash.
  Object.defineProperty(port1, 'onmessage', {
    get() {
      throw err;
    }
  });

  port2.postMessage({ foo: 'bar' });

  // We need to start the port manually because .onmessage assignment tracking
  // has been overridden.
  port1.start();
  port1.ref();
}
