'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.opensslCli) {
  common.skip('node compiled without OpenSSL CLI.');
  return;
}

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

var tls = require('tls');
var net = require('net');
var fs = require('fs');

var success = false;

function filenamePEM(n) {
  return require('path').join(common.fixturesDir, 'keys', n + '.pem');
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

var opts = {
  key: loadPEM('agent2-key'),
  cert: loadPEM('agent2-cert')
};

var max_iter = 20;
var iter = 0;

var server = tls.createServer(opts, function(s) {
  s.pipe(s);
  s.on('error', function(e) {
    // ignore error
  });
});

server.listen(0, function() {
  sendClient();
});


function sendClient() {
  var client = tls.connect(server.address().port, {
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
  var BAD_RECORD = Buffer.from([0xff, 0xff, 0xff, 0xff, 0xff, 0xff]);
  var socket = net.connect(server.address().port);
  var client = tls.connect({
    socket: socket,
    rejectUnauthorized: false
  }, function() {
    socket.write(BAD_RECORD);
    socket.end();
  });
  client.on('error', function(e) {
    // ignore error
  });
}

process.on('exit', function() {
  assert(iter === max_iter);
  assert(success);
});
