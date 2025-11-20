'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  dur: [5],
  securing: ['TLSSocket', 'clear'],
  size: [100, 1024, 1024 * 1024],
}, {
  flags: ['--no-warnings'],
});

const fixtures = require('../../test/common/fixtures');
const tls = require('tls');
const net = require('net');

const REDIRECT_PORT = 28347;

function main({ dur, size, securing }) {
  const chunk = Buffer.alloc(size, 'b');

  const options = {
    key: fixtures.readKey('rsa_private.pem'),
    cert: fixtures.readKey('rsa_cert.crt'),
    ca: fixtures.readKey('rsa_ca.crt'),
    ciphers: 'AES256-GCM-SHA384',
    isServer: true,
    requestCert: true,
    rejectUnauthorized: true,
    maxVersion: 'TLSv1.2',
  };

  const server = net.createServer(onRedirectConnection);
  server.listen(REDIRECT_PORT, () => {
    const proxy = net.createServer(onProxyConnection);
    proxy.listen(common.PORT, () => {
      const clientOptions = {
        port: common.PORT,
        ca: options.ca,
        key: options.key,
        cert: options.cert,
        isServer: false,
        rejectUnauthorized: false,
        maxVersion: options.maxVersion,
      };
      const network = securing === 'clear' ? net : tls;
      const conn = network.connect(clientOptions, () => {
        setTimeout(() => {
          const mbits = (received * 8) / (1024 * 1024);
          bench.end(mbits);
          if (conn)
            conn.destroy();
          server.close();
          proxy.close();
        }, dur * 1000);
        bench.start();
        conn.on('drain', write);
        write();
      });
      conn.on('error', (e) => {
        throw new Error(`Client error: ${e}`);
      });

      function write() {
        while (false !== conn.write(chunk));
      }
    });
  });

  function onProxyConnection(conn) {
    const client = net.connect(REDIRECT_PORT, () => {
      switch (securing) {
        case 'TLSSocket':
          secureTLSSocket(conn, client);
          break;
        case 'clear':
          conn.pipe(client);
          break;
        default:
          throw new Error('Invalid securing method');
      }
    });
  }

  function secureTLSSocket(conn, client) {
    const serverSocket = new tls.TLSSocket(conn, options);
    serverSocket.on('error', (e) => {
      throw new Error(`Socket error: ${e}`);
    });
    serverSocket.pipe(client);
  }

  let received = 0;
  function onRedirectConnection(conn) {
    conn.on('data', (chunk) => {
      received += chunk.length;
    });
  }
}
