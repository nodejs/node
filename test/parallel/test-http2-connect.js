'use strict';

const { mustCall, hasCrypto, skip, expectsError } = require('../common');
if (!hasCrypto)
  skip('missing crypto');
const { doesNotThrow, throws } = require('assert');
const { createServer, connect } = require('http2');
{
  const server = createServer();
  server.listen(0, mustCall(() => {
    const authority = `http://localhost:${server.address().port}`;
    const options = {};
    const listener = () => mustCall();

    const clients = new Set();
    doesNotThrow(() => clients.add(connect(authority)));
    doesNotThrow(() => clients.add(connect(authority, options)));
    doesNotThrow(() => clients.add(connect(authority, options, listener())));
    doesNotThrow(() => clients.add(connect(authority, listener())));

    for (const client of clients) {
      client.once('connect', mustCall((headers) => {
        client.close();
        clients.delete(client);
        if (clients.size === 0) {
          server.close();
        }
      }));
    }
  }));
}

// check for https as protocol
{
  const authority = 'https://localhost';
  doesNotThrow(() => {
    // A socket error may or may not be reported, keep this as a non-op
    // instead of a mustCall or mustNotCall
    connect(authority).on('error', () => {});
  });
}

// check for error for an invalid protocol (not http or https)
{
  const authority = 'ssh://localhost';
  throws(() => {
    connect(authority);
  }, expectsError({
    code: 'ERR_HTTP2_UNSUPPORTED_PROTOCOL',
    type: Error
  }));
}
