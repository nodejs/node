'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(function(socket) {
  socket.pipe(socket);
}).listen(0, common.mustCall(function() {
  const conn = net.connect(this.address().port);
  let received = '';

  conn.setEncoding('utf8');
  conn.write('before');
  conn.on('connect', function() {
    conn.write('after');
  });
  conn.on('data', function(buf) {
    received += buf;
    conn.end();
  });
  conn.on('end', common.mustCall(function() {
    server.close();
    assert.strictEqual(received, 'before' + 'after');
  }));
}));
