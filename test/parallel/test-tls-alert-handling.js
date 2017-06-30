'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.opensslCli)
  common.skip('node compiled without OpenSSL CLI.');

const fs = require('fs');
const net = require('net');
const path = require('path');
const tls = require('tls');

function filenamePEM(n) {
  return path.join(common.fixturesDir, 'keys', `${n}.pem`);
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

const opts = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert')
};
const max_iter = 20;
let iter = 0;

const server = tls.createServer(opts, function(s) {
  s.pipe(s);
  s.on('error', function() {
    // ignore error
  });
});

server.listen(0, function() {
  sendClient();
});


function sendClient() {
  const client = tls.connect(server.address().port, {
    rejectUnauthorized: false
  });
  client.on('data', common.mustCall(function() {
    if (iter++ === 2) sendBADTLSRecord();
    if (iter < max_iter) {
      client.write('a');
      return;
    }
    client.end();
    server.close();
  }, max_iter));
  client.write('a');
  client.on('error', function() {
    // ignore error
  });
  client.on('close', function() {
    server.close();
  });
}


function sendBADTLSRecord() {
  const BAD_RECORD = Buffer.from([0xff, 0xff, 0xff, 0xff, 0xff, 0xff]);
  const socket = net.connect(server.address().port);
  const client = tls.connect({
    socket: socket,
    rejectUnauthorized: false
  }, function() {
    socket.write(BAD_RECORD);
    socket.end();
  });
  client.on('error', function() {
    // ignore error
  });
}
