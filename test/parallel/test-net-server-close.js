'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

const events = [];
const sockets = [];

process.on('exit', function() {
  assert.strictEqual(server.connections, 0);
  assert.strictEqual(events.length, 3);
  // Expect to see one server event and two client events. The order of the
  // events is undefined because they arrive on the same event loop tick.
  assert.strictEqual(events.join(' ').match(/server/g).length, 1);
  assert.strictEqual(events.join(' ').match(/client/g).length, 2);
});

let server = net.createServer(function(c) {
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
