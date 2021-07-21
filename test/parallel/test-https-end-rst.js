'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const https = require('https');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const opt = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const data = 'hello';
const server = https.createServer(opt, common.mustCallAtLeast((req, res) => {
  res.setHeader('content-length', data.length);
  res.end(data);
}, 2));

server.listen(0, function() {
  const options = {
    host: '127.0.0.1',
    port: server.address().port,
    rejectUnauthorized: false,
    ALPNProtocols: ['http/1.1'],
    allowHalfOpen: true
  };
  const socket = tls.connect(options, common.mustCall(() => {
    socket.write('GET /\n\n');
    socket.once('data', common.mustCall(() => {
      socket.write('GET /\n\n');
      setTimeout(common.mustCall(() => {
        socket.destroy();
        server.close();
      }), common.platformTimeout(10));
    }));
  }));
});
