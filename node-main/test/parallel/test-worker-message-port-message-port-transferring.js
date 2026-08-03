'use strict';
const common = require('../common');
const assert = require('assert');

const { MessageChannel } = require('worker_threads');

{
  const { port1: basePort1, port2: basePort2 } = new MessageChannel();
  const {
    port1: transferredPort1, port2: transferredPort2
  } = new MessageChannel();

  basePort1.postMessage({ transferredPort1 }, [ transferredPort1 ]);
  basePort2.on('message', common.mustCall(({ transferredPort1 }) => {
    transferredPort1.postMessage('foobar');
    transferredPort2.on('message', common.mustCall((msg) => {
      assert.strictEqual(msg, 'foobar');
      transferredPort1.close(common.mustCall());
      basePort1.close(common.mustCall());
    }));
  }));
}
