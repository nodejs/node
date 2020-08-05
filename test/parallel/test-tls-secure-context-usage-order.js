'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that when a TLS connection is established, the server
// selects the most recently added SecureContext that matches the servername.

const assert = require('assert');
const tls = require('tls');

function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`);
}

const serverOptions = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert'),
  requestCert: true,
  rejectUnauthorized: false,
};

const badSecureContext = {
  key: loadPEM('agent1-key'),
  cert: loadPEM('agent1-cert'),
  ca: [ loadPEM('ca2-cert') ]
};

const goodSecureContext = {
  key: loadPEM('agent1-key'),
  cert: loadPEM('agent1-cert'),
  ca: [ loadPEM('ca1-cert') ]
};

const server = tls.createServer(serverOptions, (c) => {
  // The 'a' and 'b' subdomains are used to distinguish between client
  // connections.
  // Connection to subdomain 'a' is made when the 'bad' secure context is
  // the only one in use.
  if ('a.example.com' === c.servername) {
    assert.strictEqual(c.authorized, false);
  }
  // Connection to subdomain 'b' is made after the 'good' context has been
  // added.
  if ('b.example.com' === c.servername) {
    assert.strictEqual(c.authorized, true);
  }
});

// 1. Add the 'bad' secure context. A connection using this context will not be
// authorized.
server.addContext('*.example.com', badSecureContext);

server.listen(0, () => {
  const options = {
    port: server.address().port,
    key: loadPEM('agent1-key'),
    cert: loadPEM('agent1-cert'),
    ca: [loadPEM('ca1-cert')],
    servername: 'a.example.com',
    rejectUnauthorized: false,
  };

  // 2. Make a connection using servername 'a.example.com'. Since a 'bad'
  // secure context is used, this connection should not be authorized.
  const client = tls.connect(options, () => {
    client.end();
  });

  client.on('close', common.mustCall(() => {
    // 3. Add a 'good' secure context.
    server.addContext('*.example.com', goodSecureContext);

    options.servername = 'b.example.com';
    // 4. Make a connection using servername 'b.example.com'. This connection
    // should be authorized because the 'good' secure context is the most
    // recently added matching context.

    const other = tls.connect(options, () => {
      other.end();
    });

    other.on('close', common.mustCall(() => {
      // 5. Make another connection using servername 'b.example.com' to ensure
      // that the array of secure contexts is not reversed in place with each
      // SNICallback call, as someone might be tempted to refactor this piece of
      // code by using Array.prototype.reverse() method.
      const onemore = tls.connect(options, () => {
        onemore.end();
      });

      onemore.on('close', common.mustCall(() => {
        server.close();
      }));
    }));
  }));
});
