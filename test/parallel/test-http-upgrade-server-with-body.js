'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const server = http.createServer();

server.on('request', common.mustNotCall());

server.on('upgrade', function(req, socket, upgradeHead) {
  // Read the body:
  let reqBody = '';
  req.on('data', common.mustCall((str) => { reqBody += str; }));
  req.on('end', common.mustCall(() => {
    assert.strictEqual(reqBody, 'request body');
  }));

  assert.strictEqual(upgradeHead.toString(), 'upgrade head');

  // Confirm the upgrade:
  socket.write('HTTP/1.1 101 Switching Protocols\r\n' +
               'Upgrade: custom-protocol\r\n' +
               'Connection: Upgrade\r\n' +
               '\r\n');

  // Check we see the upgraded protocol data only (not the
  // request body):
  socket.on('data', common.mustCall((d) => {
    assert.strictEqual(d.toString(), 'post-upgrade message');
    socket.end();
  }));
});

server.listen(0, async function() {
  const conn = net.createConnection(this.address().port);
  conn.setEncoding('utf8');

  await new Promise((resolve) => conn.on('connect', resolve));

  // Write request headers, leave the body pending:
  conn.write(
    'POST / HTTP/1.1\r\n' +
    'host: localhost\r\n' +
    'Upgrade: custom-protocol\r\n' +
    'Connection: Upgrade\r\n' +
    'transfer-encoding: chunked\r\n' +
    '\r\n'
  );

  await new Promise((resolve) => setTimeout(resolve, 10));

  // Write the body and part of the initial upgrade data. Most clients
  // shouldn't send this until after 101 response, but we want to make
  // sure it works even if weird cases.
  conn.write(
    'C\r\nrequest body\r\n' +
    '0\r\n\r\n' +
    'upgrade head'
  );

  const response = await new Promise((resolve) => conn.once('data', resolve));
  assert.ok(response.startsWith('HTTP/1.1 101 Switching Protocols\r\n'));

  conn.write('post-upgrade message');

  await new Promise((resolve) => conn.on('end', resolve));

  conn.end();
  server.close();
});
