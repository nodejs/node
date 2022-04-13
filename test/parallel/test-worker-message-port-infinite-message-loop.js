'use strict';
const common = require('../common');
const assert = require('assert');

const { MessageChannel } = require('worker_threads');

// Make sure that an infinite asynchronous .on('message')/postMessage loop
// does not lead to a stack overflow and does not starve the event loop.
// We schedule timeouts both from before the .on('message') handler and
// inside of it, which both should run.

const { port1, port2 } = new MessageChannel();
let count = 0;
port1.on('message', () => {
  if (count === 0) {
    setTimeout(common.mustCall(() => {
      port1.close();
    }), 0);
  }

  port2.postMessage(0);
  assert(count++ < 10000, `hit ${count} loop iterations`);
});

port2.postMessage(0);

// This is part of the test -- the event loop should be available and not stall
// out due to the recursive .postMessage() calls.
setTimeout(common.mustCall(), 0);
