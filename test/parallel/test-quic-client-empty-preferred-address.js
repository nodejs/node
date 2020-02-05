// Flags: --no-warnings
'use strict';

// This test ensures that when we don't define `preferredAddress`
// on the server while the `preferredAddressPolicy` on the client
// is `accpet`, it works as expected.

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { key, cert, ca } = require('../common/quic');
const { createSocket } = require('quic');

const kServerName = 'agent2';
const server = createSocket();

let client;
const options = { key, cert, ca, alpn: 'zzz' };
server.listen(options);

server.on('session', common.mustCall((serverSession) => {
  serverSession.on('stream', common.mustCall((stream) => {
    stream.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString('utf8'), 'hello');
    }));
    stream.on('end', common.mustCall(() => {
      stream.close();
      client.close();
      server.close();
    }));
  }));
}));

server.on('ready', common.mustCall(() => {
  client = createSocket({ client: options });

  const clientSession = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
    servername: kServerName,
    preferredAddressPolicy: 'accept',
  });

  clientSession.on('close', common.mustCall());

  clientSession.on('secure', common.mustCall(() => {
    const stream = clientSession.openStream();
    stream.end('hello');
    stream.on('close', common.mustCall());
  }));
}));
