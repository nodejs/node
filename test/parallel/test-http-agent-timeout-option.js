'use strict';

const { mustCall } = require('../common');
const { strictEqual } = require('assert');
const { Agent, get } = require('http');

// Test that the listener that forwards the `'timeout'` event from the socket to
// the `ClientRequest` instance is added to the socket when the `timeout` option
// of the `Agent` is set.

const request = get({
  agent: new Agent({ timeout: 50 }),
  lookup: () => {}
});

request.on('socket', mustCall((socket) => {
  strictEqual(socket.timeout, 50);

  const listeners = socket.listeners('timeout');

  strictEqual(listeners.length, 2);
  strictEqual(listeners[1], request.timeoutCb);
}));
