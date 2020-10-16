// Flags: --expose-internals
'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.hasMultiLocalhost())
  common.skip('platform-specific test.');

const http2 = require('http2');
const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const serverOptions = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};
const server = http2.createSecureServer(serverOptions, (req, res) => {
  console.log(`Connect from: ${req.connection.remoteAddress}`);
  assert.strictEqual(req.connection.remoteAddress, '127.0.0.2');

  req.on('end', common.mustCall(() => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end(`You are from: ${req.connection.remoteAddress}`);
  }));
  req.resume();
});

server.listen(0, '127.0.0.1', common.mustCall(() => {
  const options = {
    ALPNProtocols: ['h2'],
    host: '127.0.0.1',
    servername: 'localhost',
    localAddress: '127.0.0.2',
    port: server.address().port,
    rejectUnauthorized: false
  };

  console.log('Server ready', server.address().port);

  const socket = tls.connect(options, async () => {

    console.log('TLS Connected!');

    setTimeout(() => {

      const client = http2.connect(
        'https://localhost:' + server.address().port,
        { ...options, createConnection: () => socket }
      );
      const req = client.request({
        ':path': '/'
      });
      req.on('data', () => req.resume());
      req.on('end', common.mustCall(function() {
        client.close();
        req.close();
        server.close();
      }));
      req.end();
    }, 1000);
  });
}));
