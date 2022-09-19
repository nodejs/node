'use strict';
const common = require('../common');

// Regression test for https://github.com/nodejs/node/issues/11788.

const assert = require('assert');
const http = require('http');
const net = require('net');

for (const enc of ['utf8', 'utf16le', 'latin1', 'UTF-8']) {
  const server = http.createServer(common.mustCall((req, res) => {
    res.setHeader('content-type', `text/plain; charset=${enc}`);
    res.write('helloworld', enc);
    res.end();
  })).listen(0);

  server.on('listening', common.mustCall(() => {
    const buffers = [];
    const socket = net.connect(server.address().port);
    socket.write('GET / HTTP/1.0\r\n\r\n');
    socket.on('data', (data) => buffers.push(data));
    socket.on('end', common.mustCall(() => {
      const received = Buffer.concat(buffers);
      const headerEnd = received.indexOf('\r\n\r\n', 'utf8');
      assert.notStrictEqual(headerEnd, -1);

      const header = received.toString('utf8', 0, headerEnd).split('\r\n');
      const body = received.toString(enc, headerEnd + 4);

      assert.strictEqual(header[0], 'HTTP/1.1 200 OK');
      assert.strictEqual(header[1], `content-type: text/plain; charset=${enc}`);
      assert.strictEqual(body, 'helloworld');
      server.close();
    }));
  }));
}
