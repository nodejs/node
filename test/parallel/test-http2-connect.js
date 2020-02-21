'use strict';

const {
  mustCall,
  hasCrypto,
  hasIPv6,
  skip,
  expectsError
} = require('../common');
if (!hasCrypto)
  skip('missing crypto');
const assert = require('assert');
const { createServer, connect } = require('http2');
const { connect: netConnect } = require('net');

// Check for session connect callback and event
{
  const server = createServer();
  server.listen(0, mustCall(() => {
    const authority = `http://localhost:${server.address().port}`;
    const options = {};
    const listener = () => mustCall();

    const clients = new Set();
    // Should not throw.
    clients.add(connect(authority));
    clients.add(connect(authority, options));
    clients.add(connect(authority, options, listener()));
    clients.add(connect(authority, listener()));

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

// Check for session connect callback on already connected socket
{
  const server = createServer();
  server.listen(0, mustCall(() => {
    const { port } = server.address();

    const onSocketConnect = () => {
      const authority = `http://localhost:${port}`;
      const createConnection = mustCall(() => socket);
      const options = { createConnection };
      connect(authority, options, mustCall(onSessionConnect));
    };

    const onSessionConnect = (session) => {
      session.close();
      server.close();
    };

    const socket = netConnect(port, mustCall(onSocketConnect));
  }));
}

// Check for https as protocol
{
  const authority = 'https://localhost';
  // A socket error may or may not be reported, keep this as a non-op
  // instead of a mustCall or mustNotCall
  connect(authority).on('error', () => {});
}

// Check for error for init settings error
{
  createServer(function() {
    connect(`http://localhost:${this.address().port}`, {
      settings: {
        maxFrameSize: 1   // An incorrect settings
      }
    }).on('error', expectsError({
      code: 'ERR_HTTP2_INVALID_SETTING_VALUE',
      name: 'RangeError'
    }));
  });
}

// Check for error for an invalid protocol (not http or https)
{
  const authority = 'ssh://localhost';
  assert.throws(() => {
    connect(authority);
  }, {
    code: 'ERR_HTTP2_UNSUPPORTED_PROTOCOL',
    name: 'Error'
  });
}

// Check for literal IPv6 addresses in URL's
if (hasIPv6) {
  const server = createServer();
  server.listen(0, '::1', mustCall(() => {
    const { port } = server.address();
    const clients = new Set();

    clients.add(connect(`http://[::1]:${port}`));
    clients.add(connect(new URL(`http://[::1]:${port}`)));

    for (const client of clients) {
      client.once('connect', mustCall(() => {
        client.close();
        clients.delete(client);
        if (clients.size === 0) {
          server.close();
        }
      }));
    }
  }));
}

// Check that `options.host` and `options.port` take precedence over
// `authority.host` and `authority.port`.
{
  const server = createServer();
  server.listen(0, mustCall(() => {
    connect('http://foo.bar', {
      host: 'localhost',
      port: server.address().port
    }, mustCall((session) => {
      session.close();
      server.close();
    }));
  }));
}
