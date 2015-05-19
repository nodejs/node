'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');
var net = require('net');
var crypto = require('crypto');

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

var servers = [ createServer(), createServer(),
                createServer(), createServer(),
                createServer(), createServer() ];

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
