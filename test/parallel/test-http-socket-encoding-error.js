'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer().listen(0, connectToServer);

server.on('connection', common.mustCall((socket) => {
  assert.throws(
    () => {
      socket.setEncoding('');
    },
    {
      code: 'ERR_HTTP_SOCKET_ENCODING',
      name: 'Error',
    }
  );

  socket.end();
}));

function connectToServer() {
  const client = new http.Agent().createConnection(this.address().port, () => {
    client.end();
  }).on('end', () => server.close());
}
