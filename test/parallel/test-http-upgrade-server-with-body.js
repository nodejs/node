'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const server = http.createServer();

server.on('request', common.mustNotCall());

server.on('upgrade', function(req, socket, upgradeHead) {
  let reqBody = '';
  req.on('data', common.mustCall((str) => { reqBody += str; }));
  req.on('end', common.mustCall(() => {
    assert.strictEqual(reqBody, 'request body');
    socket.end();
  }));

  assert.strictEqual(upgradeHead.toString(), 'upgrade head');
});

server.listen(0, function() {
  const conn = net.createConnection(this.address().port);
  conn.setEncoding('utf8');

  conn.on('connect', function() {
    conn.write(
      'POST / HTTP/1.1\r\n' +
      'Upgrade: WebSocket\r\n' +
      'Connection: Upgrade\r\n' +
      'Content-Length: 12\r\n' +
      '\r\n' +
      'request body' +
      'upgrade head'
    );
  });

  conn.on('end', function() {
    conn.end();
  });

  conn.on('close', function() {
    server.close();
  });
});
