'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const expected = 'hello1hello2hello3\nbye';

const server = net.createServer({
  allowHalfOpen: true
}, common.mustCall(function(sock) {
  let serverData = '';

  sock.setEncoding('utf8');
  sock.on('data', function(c) {
    serverData += c;
  });
  sock.on('end', common.mustCall(function() {
    assert.strictEqual(serverData, expected);
    sock.end(serverData);
    server.close();
  }));
}));
server.listen(0, common.mustCall(function() {
  const sock = net.connect(this.address().port);
  let clientData = '';

  sock.setEncoding('utf8');
  sock.on('data', function(c) {
    clientData += c;
  });

  sock.on('end', common.mustCall(function() {
    assert.strictEqual(clientData, expected);
  }));

  sock.write('hello1');
  sock.write('hello2');
  sock.write('hello3\n');
  assert.strictEqual(sock.end('bye'), sock);

}));
