'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const https = require('https');
const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');
const net = require('net');
const { Duplex } = require('stream');

class CustomAgent extends https.Agent {
  createConnection(options, cb) {
    const realSocket = net.createConnection(options);
    const stream = new Duplex({
      emitClose: false,
      read(n) {
        (function retry() {
          const data = realSocket.read();
          if (data === null)
            return realSocket.once('readable', retry);
          stream.push(data);
        })();
      },
      write(chunk, enc, callback) {
        realSocket.write(chunk, enc, callback);
      },
    });
    realSocket.on('end', () => stream.push(null));

    stream.on('end', common.mustCall());
    return tls.connect({ ...options, socket: stream });
  }
}

const httpsServer = https.createServer({
  key,
  cert,
}, (req, res) => {
  httpsServer.close();
  res.end('hello world!');
});
httpsServer.listen(0, 'localhost', () => {
  const agent = new CustomAgent();
  https.get({
    host: 'localhost',
    port: httpsServer.address().port,
    agent,
    headers: { Connection: 'close' },
    rejectUnauthorized: false
  }, (res) => {
    res.resume();
  });
});
