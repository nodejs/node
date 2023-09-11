'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const COUNT = 1000 + 1;
let gotResponses = 0;

const response = Buffer.from('HTTP/1.1 200 OK\r\n\r\n');

function execAndClose() {
  process.stdout.write('.');

  const chunks = [];
  const socket = net.connect(common.PORT);

  socket.on('end', socket.end);
  socket.on('connect', function() {
    process.stdout.write('c');
  });
  socket.on('data', function(chunk) {
    process.stdout.write('d');
    chunks.push(chunk);
  });
  socket.on('close', function() {
    assert.deepStrictEqual(Buffer.concat(chunks), response);

    if (++gotResponses === COUNT) {
      server.close();
    } else {
      execAndClose();
    }
  });
}

const server = net.createServer(function(socket) {
  socket.end(response);
  socket.resume();
});

server.listen(common.PORT, common.localhostIPv4, common.mustCall(execAndClose));
