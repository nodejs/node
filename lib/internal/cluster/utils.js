'use strict';

module.exports = {
  sendHelper,
  internal
};

const callbacks = new Map();
var seq = 0;

function sendHelper(proc, message, handle, cb) {
  if (!proc.connected)
    return false;

  // Mark message as internal. See INTERNAL_PREFIX in lib/child_process.js
  message = { cmd: 'NODE_CLUSTER', ...message, seq };

  if (typeof cb === 'function')
    callbacks.set(seq, cb);

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

    if (message.ack !== undefined) {
      const callback = callbacks.get(message.ack);

      if (callback !== undefined) {
        fn = callback;
        callbacks.delete(message.ack);
      }
    }

    fn.apply(worker, arguments);
  };
}
