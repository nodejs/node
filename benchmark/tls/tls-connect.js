'use strict';
const fixtures = require('../../test/common/fixtures');
const tls = require('tls');

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  concurrency: [1, 10],
  dur: [5]
});

let clientConn = 0;
let serverConn = 0;
let dur;
let concurrency;
let running = true;

function main(conf) {
  dur = conf.dur;
  concurrency = conf.concurrency;
  const options = {
    key: fixtures.readKey('rsa_private.pem'),
    cert: fixtures.readKey('rsa_cert.crt'),
    ca: fixtures.readKey('rsa_ca.crt'),
    ciphers: 'AES256-GCM-SHA384'
  };

  const server = tls.createServer(options, onConnection);
  server.listen(common.PORT, onListening);
}

function onListening() {
  setTimeout(done, dur * 1000);
  bench.start();
  for (let i = 0; i < concurrency; i++)
    makeConnection();
}

function onConnection(conn) {
  serverConn++;
}

function makeConnection() {
  const options = {
    port: common.PORT,
    rejectUnauthorized: false
  };
  const conn = tls.connect(options, () => {
    clientConn++;
    conn.on('error', (er) => {
      console.error('client error', er);
      throw er;
    });
    conn.end();
    if (running) makeConnection();
  });
}

function done() {
  running = false;
  // It's only an established connection if they both saw it.
  // because we destroy the server somewhat abruptly, these
  // don't always match.  Generally, serverConn will be
  // the smaller number, but take the min just to be sure.
  bench.end(Math.min(serverConn, clientConn));
  process.exit(0);
}
