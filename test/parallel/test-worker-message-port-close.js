'use strict';
const common = require('../common');
const assert = require('assert');
const { MessageChannel, moveMessagePortToContext } = require('worker_threads');

// Make sure that .start() and .stop() do not throw on closing/closed
// MessagePorts.
// Refs: https://github.com/nodejs/node/issues/26463

function dummy() {}

{
  const { port1, port2 } = new MessageChannel();
  port1.close(common.mustCall(() => {
    port1.on('message', dummy);
    port1.off('message', dummy);
    port2.on('message', dummy);
    port2.off('message', dummy);
  }));
  port1.on('message', dummy);
  port1.off('message', dummy);
  port2.on('message', dummy);
  port2.off('message', dummy);
}

{
  const { port1 } = new MessageChannel();
  port1.on('message', dummy);
  port1.close(common.mustCall(() => {
    port1.off('message', dummy);
  }));
}

{
  const { port2 } = new MessageChannel();
  port2.close();
  assert.throws(() => moveMessagePortToContext(port2, {}), {
    code: 'ERR_CLOSED_MESSAGE_PORT',
    message: 'Cannot send data on closed MessagePort'
  });
}

// Refs: https://github.com/nodejs/node/issues/42296
{
  const ch = new MessageChannel();
  ch.port1.onmessage = common.mustNotCall();
  ch.port2.close();
  ch.port2.postMessage('fhqwhgads');
}
