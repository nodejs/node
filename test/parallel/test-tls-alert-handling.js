'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.opensslCli) {
  common.skip('node compiled without OpenSSL CLI.');
  return;
}

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const tls = require('tls');
const net = require('net');
const fs = require('fs');

let success = false;

function filenamePEM(n) {
  return require('path').join(common.fixturesDir, 'keys', n + '.pem');
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

let opts = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert')
};
const max_iter = 20;
let iter = 0;

let server = tls.createServer(opts, function(s) {
  s.pipe(s);
  s.on('error', function(e) {
    // ignore error
  });
});

server.listen(0, function() {
  sendClient();
});


function sendClient() {
  let client = tls.connect(server.address().port, {
    rejectUnauthorized: false
  });
  client.on('data', function(chunk) {
    if (iter++ === 2) sendBADTLSRecord();
    if (iter < max_iter) {
      client.write('a');
      return;
    }
    client.end();
    server.close();
    success = true;
  });
  client.write('a');
  client.on('error', function(e) {
    // ignore error
  });
  client.on('close', function() {
    server.close();
  });
}


function sendBADTLSRecord() {
  let BAD_RECORD = Buffer.from([0xff, 0xff, 0xff, 0xff, 0xff, 0xff]);
  let socket = net.connect(server.address().port);
  let client = tls.connect({
    socket: socket,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    socket.write(BAD_RECORD);
    socket.end();
  }, 1));
  client.on('error', function(e) {
    // ignore error
  });
}

process.on('exit', function() {
  assert.strictEqual(iter, max_iter);
  assert(success);
});
