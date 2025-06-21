'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { Duplex } = require('stream');

const server = h2.createSecureServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
});

class JSSocket extends Duplex {
  constructor(socket) {
    super({ emitClose: true });
    socket.on('close', () => this.destroy());
    socket.on('data', (data) => this.push(data));
    this.socket = socket;
  }

  _write(data, encoding, callback) {
    this.socket.write(data, encoding, callback);
  }

  _read(size) {
  }

  _final(cb) {
    cb();
  }
}

server.listen(0, common.mustCall(function() {
  const socket = tls.connect({
    rejectUnauthorized: false,
    host: 'localhost',
    port: this.address().port,
    ALPNProtocols: ['h2']
  }, () => {
    const proxy = new JSSocket(socket);
    const client = h2.connect(`https://localhost:${this.address().port}`, {
      createConnection: () => proxy
    });
    const req = client.request();

    server.on('request', () => {
      socket.destroy();
    });

    req.on('close', common.mustCall(() => {
      client.close();
      server.close();
    }));
  });
}));
