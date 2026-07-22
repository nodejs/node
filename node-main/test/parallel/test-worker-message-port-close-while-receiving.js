'use strict';
const common = require('../common');

const { MessageChannel } = require('worker_threads');

// Make sure that closing a message port while receiving messages on it does
// not stop messages that are already in the queue from being emitted.

const { port1, port2 } = new MessageChannel();

port1.on('message', common.mustCall(() => {
  port1.close();
}, 2));
port2.postMessage('foo');
port2.postMessage('bar');
