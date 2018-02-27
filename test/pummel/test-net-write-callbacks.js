'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');

let cbcount = 0;
const N = 500000;

const server = net.Server(function(socket) {
  socket.on('data', function(d) {
    console.error(`got ${d.length} bytes`);
  });

  socket.on('end', function() {
    console.error('end');
    socket.destroy();
    server.close();
  });
});

let lastCalled = -1;
function makeCallback(c) {
  let called = false;
  return function() {
    if (called)
      throw new Error(`called callback #${c} more than once`);
    called = true;
    if (c < lastCalled)
      throw new Error(
        `callbacks out of order. last=${lastCalled} current=${c}`);
    lastCalled = c;
    cbcount++;
  };
}

server.listen(common.PORT, function() {
  const client = net.createConnection(common.PORT);

  client.on('connect', function() {
    for (let i = 0; i < N; i++) {
      client.write('hello world', makeCallback(i));
    }
    client.end();
  });
});

process.on('exit', function() {
  assert.strictEqual(N, cbcount);
});
