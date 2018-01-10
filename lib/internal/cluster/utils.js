'use strict';
const util = require('util');

module.exports = {
  sendHelper,
  internal,
  hash,
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

function hash(str) {
  var hash = 5381,
    i = str.length;

  while (i) {
    hash = (hash * 33) ^ str.charCodeAt(--i);
  }

  /* JavaScript does bitwise operations (like XOR, above) on 32-bit signed
   * integers. Since we want the results to be always positive, convert the
   * signed int to an unsigned by doing an unsigned bitshift. */
  return hash >>> 0;
}
