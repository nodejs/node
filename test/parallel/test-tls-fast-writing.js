'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');

var PORT = common.PORT;
var dir = common.fixturesDir;
var options = { key: fs.readFileSync(dir + '/test_key.pem'),
                cert: fs.readFileSync(dir + '/test_cert.pem'),
                ca: [ fs.readFileSync(dir + '/test_ca.pem') ] };

var server = tls.createServer(options, onconnection);
var gotChunk = false;
var gotDrain = false;

var timer = setTimeout(function() {
  console.log('not ok - timed out');
  process.exit(1);
}, common.platformTimeout(500));

function onconnection(conn) {
  conn.on('data', function(c) {
    if (!gotChunk) {
      gotChunk = true;
      console.log('ok - got chunk');
    }

    // just some basic sanity checks.
    assert(c.length);
    assert(Buffer.isBuffer(c));

    if (gotDrain)
      process.exit(0);
  });
}

server.listen(PORT, function() {
  var chunk = new Buffer(1024);
  chunk.fill('x');
  var opt = { port: PORT, rejectUnauthorized: false };
  var conn = tls.connect(opt, function() {
    conn.on('drain', ondrain);
    write();
  });
  function ondrain() {
    if (!gotDrain) {
      gotDrain = true;
      console.log('ok - got drain');
    }
    if (gotChunk)
      process.exit(0);
    write();
  }
  function write() {
    // this needs to return false eventually
    while (false !== conn.write(chunk));
  }
});
