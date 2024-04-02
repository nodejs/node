'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

['on', 'addListener', 'prependListener'].forEach((testFn) => {
  let received = '';

  const server = http.createServer(function(req, res) {
    res.writeHead(200);
    res.end();

    req.socket[testFn]('data', function(data) {
      received += data;
    });

    server.close();
  }).listen(0, function() {
    const socket = net.connect(this.address().port, function() {
      socket.write('PUT / HTTP/1.1\r\n\r\n');

      socket.once('data', function() {
        socket.end('hello world');
      });

      socket.on('end', common.mustCall(() => {
        assert.strictEqual(received, 'hello world',
                           `failed for socket.${testFn}`);
      }));
    });
  });
});
