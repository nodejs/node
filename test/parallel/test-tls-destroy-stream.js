'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const makeDuplexPair = require('../common/duplexpair');
const net = require('net');
const assert = require('assert');
const tls = require('tls');

// This test ensures that an instance of StreamWrap should emit "end" and
// "close" when the socket on the other side call `destroy()` instead of
// `end()`.
// Refs: https://github.com/nodejs/node/issues/14605
const CONTENT = 'Hello World';
const tlsServer = tls.createServer(
  {
    key: fixtures.readSync('test_key.pem'),
    cert: fixtures.readSync('test_cert.pem'),
    ca: [fixtures.readSync('test_ca.pem')],
  },
  (socket) => {
    socket.on('error', common.mustNotCall());
    socket.on('close', common.mustCall());
    socket.write(CONTENT);
    socket.destroy();
  },
);

const server = net.createServer((conn) => {
  conn.on('error', common.mustNotCall());
  // Assume that we want to use data to determine what to do with connections.
  conn.once('data', common.mustCall((chunk) => {
    const { clientSide, serverSide } = makeDuplexPair();
    serverSide.on('close', common.mustCall(() => {
      conn.destroy();
    }));
    clientSide.pipe(conn);
    conn.pipe(clientSide);

    conn.on('close', common.mustCall(() => {
      clientSide.destroy();
    }));
    clientSide.on('close', common.mustCall(() => {
      conn.destroy();
    }));

    process.nextTick(() => {
      conn.unshift(chunk);
    });

    tlsServer.emit('connection', serverSide);
  }));
});

server.listen(0, () => {
  const port = server.address().port;
  const conn = tls.connect({ port, rejectUnauthorized: false }, () => {
    conn.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString('utf8'), CONTENT);
    }));
    conn.on('error', common.mustNotCall());
    conn.on(
      'close',
      common.mustCall(() => server.close()),
    );
  });
});
