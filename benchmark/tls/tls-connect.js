'use strict';
var fs = require('fs'),
  path = require('path'),
  tls = require('tls');

var common = require('../common.js');
var bench = common.createBenchmark(main, {
  concurrency: [1, 10],
  dur: [5]
});

var clientConn = 0;
var serverConn = 0;
var server;
var dur;
var concurrency;
var running = true;

function main(conf) {
  dur = +conf.dur;
  concurrency = +conf.concurrency;

  var cert_dir = path.resolve(__dirname, '../../test/fixtures');
  var options = {
    key: fs.readFileSync(cert_dir + '/test_key.pem'),
    cert: fs.readFileSync(cert_dir + '/test_cert.pem'),
    ca: [ fs.readFileSync(cert_dir + '/test_ca.pem') ],
    ciphers: 'AES256-GCM-SHA384'
  };

  server = tls.createServer(options, onConnection);
  server.listen(common.PORT, onListening);
}

function onListening() {
  setTimeout(done, dur * 1000);
  bench.start();
  for (var i = 0; i < concurrency; i++)
    makeConnection();
}

function onConnection(conn) {
  serverConn++;
}

function makeConnection() {
  var options = {
    port: common.PORT,
    rejectUnauthorized: false
  };
  var conn = tls.connect(options, function() {
    clientConn++;
    conn.on('error', function(er) {
      console.error('client error', er);
      throw er;
    });
    conn.end();
    if (running) makeConnection();
  });
}

function done() {
  running = false;
  // it's only an established connection if they both saw it.
  // because we destroy the server somewhat abruptly, these
  // don't always match.  Generally, serverConn will be
  // the smaller number, but take the min just to be sure.
  bench.end(Math.min(serverConn, clientConn));
  process.exit(0);
}
