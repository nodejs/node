'use strict';

const { expectsError, mustCall } = require('../common');
const { Agent, get, createServer } = require('http');

// Test that the `'timeout'` event is emitted on the `ClientRequest` instance
// when the socket timeout set via the `timeout` option of the `Agent` expires.

const server = createServer(mustCall(() => {
  // Never respond.
}));

server.listen(() => {
  const request = get({
    agent: new Agent({ timeout: 500 }),
    port: server.address().port
  });

  request.on('error', expectsError({
    type: Error,
    code: 'ECONNRESET',
    message: 'socket hang up'
  }));

  request.on('timeout', mustCall(() => {
    request.abort();
    server.close();
  }));
});
