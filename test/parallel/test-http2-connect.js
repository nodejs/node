// Flags: --expose-http2
'use strict';

const { mustCall, hasCrypto, skip } = require('../common');
if (!hasCrypto)
  skip('missing crypto');
const { doesNotThrow } = require('assert');
const { createServer, connect } = require('http2');

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
      client.destroy();
      clients.delete(client);
      if (clients.size === 0) {
        server.close();
      }
    }));
  }
}));
