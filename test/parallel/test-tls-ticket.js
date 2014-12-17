// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var assert = require('assert');
var fs = require('fs');
var net = require('net');
var tls = require('tls');
var crypto = require('crypto');

var common = require('../common');

var keys = crypto.randomBytes(48);
var serverLog = [];
var ticketLog = [];

var serverCount = 0;
function createServer() {
  var id = serverCount++;

  var server = tls.createServer({
    key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
    cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem'),
    ticketKeys: keys
  }, function(c) {
    serverLog.push(id);
    c.end();
  });

  return server;
}

var servers = [ createServer(), createServer(), createServer(), createServer(), createServer(), createServer() ];

// Create one TCP server and balance sockets to multiple TLS server instances
var shared = net.createServer(function(c) {
  servers.shift().emit('connection', c);
}).listen(common.PORT, function() {
  start(function() {
    shared.close();
  });
});

function start(callback) {
  var sess = null;
  var left = servers.length;

  function connect() {
    var s = tls.connect(common.PORT, {
      session: sess,
      rejectUnauthorized: false
    }, function() {
      sess = s.getSession() || sess;
      ticketLog.push(s.getTLSTicket().toString('hex'));
    });
    s.on('close', function() {
      if (--left === 0)
        callback();
      else
        connect();
    });
  }

  connect();
}

process.on('exit', function() {
  assert.equal(ticketLog.length, serverLog.length);
  for (var i = 0; i < serverLog.length - 1; i++) {
    assert.notEqual(serverLog[i], serverLog[i + 1]);
    assert.equal(ticketLog[i], ticketLog[i + 1]);
  }
});
