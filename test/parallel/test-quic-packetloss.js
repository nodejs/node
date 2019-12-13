'use strict';

// This test is not yet working correctly because data
// retransmission and the requisite data buffering is
// not yet working correctly
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const Countdown = require('../common/countdown');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');
const { debuglog } = require('util');
const debug = debuglog('test');

const { createSocket } = require('quic');

let client;
const server = createSocket({ type: 'udp4', port: 0 });

const kServerName = 'agent1';
const kALPN = 'echo';

const kData = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ';

const countdown = new Countdown(1, () => {
  debug('Countdown expired. Destroying sockets');
  server.close();
  client.close();
});

server.listen({
  key,
  cert,
  ca,
  alpn: kALPN
});
server.on('session', common.mustCall((session) => {
  debug('QuicServerSession Created');

  session.on('stream', common.mustCall((stream) => {
    debug('Bidirectional, Client-initiated stream %d received', stream.id);
    stream.pipe(stream);
  }));

}));

server.on('ready', common.mustCall(() => {
  debug('Server is listening on port %d', server.address.port);
  client = createSocket({ port: 0 });

  const req = client.connect({
    address: 'localhost',
    key,
    cert,
    ca,
    alpn: kALPN,
    port: server.address.port,
    servername: kServerName,
  });

  req.on('secure', common.mustCall((servername, alpn, cipher) => {
    debug('QuicClientSession TLS Handshake Complete');

    // Set for 20% received packet loss on the server
    server.setDiagnosticPacketLoss({ rx: 0.2 });

    const stream = req.openStream();

    let n = 0;
    function sendChunk() {
      if (n < kData.length) {
        stream.write(kData[n++], common.mustCall());
        setImmediate(sendChunk);
      } else {
        stream.end();
      }
    }
    sendChunk();

    let data = '';
    stream.resume();
    stream.setEncoding('utf8');
    stream.on('data', (chunk) => data += chunk);
    stream.on('end', common.mustCall(() => {
      console.log(data, data.length);
      assert.strictEqual(data, kData);
    }));

    stream.on('close', common.mustCall(() => {
      debug('Bidirectional, Client-initiated stream %d closed', stream.id);
      countdown.dec();
    }));

    debug('Bidirectional, Client-initiated stream %d opened', stream.id);
  }));
}));
