'use strict';

const { expectsError, mustCall } = require('../common');
const { Agent, get } = require('http');

// Test that the `'timeout'` event is emitted on the `ClientRequest` instance
// when the socket timeout set via the `timeout` option of the `Agent` expires.

const request = get({
  agent: new Agent({ timeout: 500 }),
  // Non-routable IP address to prevent the connection from being established.
  host: '192.0.2.1'
});

request.on('error', expectsError({
  type: Error,
  code: 'ECONNRESET',
  message: 'socket hang up'
}));

request.on('timeout', mustCall(() => {
  request.abort();
}));
