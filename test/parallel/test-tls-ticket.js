'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
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

  var counter = 0;
  var previousKey = null;

  var server = tls.createServer({
    key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
    cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem'),
    ticketKeys: keys
  }, function(c) {
    serverLog.push(id);
    c.end();

    counter++;

    // Rotate ticket keys
    if (counter === 1) {
      previousKey = server.getTicketKeys();
      server.setTicketKeys(crypto.randomBytes(48));
    } else if (counter === 2) {
      server.setTicketKeys(previousKey);
    } else if (counter === 3) {
      // Use keys from counter=2
    } else {
      throw new Error('UNREACHABLE');
    }
  });

  return server;
}

var naturalServers = [ createServer(), createServer(), createServer() ];

// 3x servers
var servers = naturalServers.concat(naturalServers).concat(naturalServers);

// Create one TCP server and balance sockets to multiple TLS server instances
var shared = net.createServer(function(c) {
  servers.shift().emit('connection', c);
}).listen(0, function() {
  start(function() {
    shared.close();
  });
});

function start(callback) {
  var sess = null;
  var left = servers.length;

  function connect() {
    var s = tls.connect(shared.address().port, {
      session: sess,
      rejectUnauthorized: false
    }, function() {
      sess = sess || s.getSession();
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
  for (var i = 0; i < naturalServers.length - 1; i++) {
    assert.notEqual(serverLog[i], serverLog[i + 1]);
    assert.equal(ticketLog[i], ticketLog[i + 1]);

    // 2nd connection should have different ticket
    assert.notEqual(ticketLog[i], ticketLog[i + naturalServers.length]);

    // 3rd connection should have the same ticket
    assert.equal(ticketLog[i], ticketLog[i + naturalServers.length * 2]);
  }
});
