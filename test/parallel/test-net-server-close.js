'use strict';
require('../common');
var assert = require('assert');
var net = require('net');

var events = [];
var sockets = [];

process.on('exit', function() {
  assert.equal(server.connections, 0);
  assert.equal(events.length, 3);
  // Expect to see one server event and two client events. The order of the
  // events is undefined because they arrive on the same event loop tick.
  assert.equal(events.join(' ').match(/server/g).length, 1);
  assert.equal(events.join(' ').match(/client/g).length, 2);
});

var server = net.createServer(function(c) {
  c.on('close', function() {
    events.push('client');
  });

  sockets.push(c);

  if (sockets.length === 2) {
    server.close();
    sockets.forEach(function(c) { c.destroy(); });
  }
});

server.on('close', function() {
  events.push('server');
});

server.listen(0, function() {
  net.createConnection(this.address().port);
  net.createConnection(this.address().port);
});
