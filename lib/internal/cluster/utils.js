'use strict';
const util = require('util');

module.exports = {
  sendHelper,
  internal,
  handles: {} // Used in tests.
};

const callbacks = {};
var seq = 0;

function sendHelper(proc, message, handle, cb) {
  if (!proc.connected)
    return false;

  // Mark message as internal. See INTERNAL_PREFIX in lib/child_process.js
  message = util._extend({ cmd: 'NODE_CLUSTER' }, message);

  if (typeof cb === 'function')
    callbacks[seq] = cb;

  message.seq = seq;
  seq += 1;
  return proc.send(message, handle);
}

// Returns an internalMessage listener that hands off normal messages
// to the callback but intercepts and redirects ACK messages.
function internal(worker, cb) {
  return function onInternalMessage(message, handle) {
    if (message.cmd !== 'NODE_CLUSTER')
      return;

    var fn = cb;

    if (message.ack !== undefined && callbacks[message.ack] !== undefined) {
      fn = callbacks[message.ack];
      delete callbacks[message.ack];
    }

    fn.apply(worker, arguments);
  };
}
