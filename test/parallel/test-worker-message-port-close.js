'use strict';
const common = require('../common');
const { MessageChannel } = require('worker_threads');

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
