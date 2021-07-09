'use strict';
const common = require('../common');
const assert = require('assert');

const { createServer } = require('http');
const { connect } = require('net');

let connections = 0;

const server = createServer(common.mustCall(function(req, res) {
  res.writeHead(200, { Connection: 'keep-alive' });
  res.end();
}), {
  headersTimeout: 0,
  keepAliveTimeout: 0,
  requestTimeout: common.platformTimeout(60000),
});

server.on('connection', function() {
  connections++;
});

server.listen(0, function() {
  const port = server.address().port;

  // Create a first request but never finish it
  const client1 = connect(port);

  client1.on('connect', common.mustCall(() => {
    // Create a second request, let it finish but leave the connection opened using HTTP keep-alive
    const client2 = connect(port);
    let response = '';

    client2.setEncoding('utf8');

    client2.on('data', common.mustCall((chunk) => {
      response += chunk;

      if (response.endsWith('0\r\n\r\n')) {
        assert(response.startsWith('HTTP/1.1 200 OK\r\nConnection: keep-alive'));
        assert.strictEqual(connections, 2);

        server.closeAllConnections();
        server.close(common.mustCall());

        // This timer should never go off as the server.close should shut everything down
        setTimeout(common.mustNotCall(), common.platformTimeout(1500)).unref();
      }
    }));

    client2.on('close', common.mustCall());

    client2.write('GET / HTTP/1.1\r\nHost: example.com\r\n\r\n');
  }));

  client1.on('close', common.mustCall());

  client1.on('error', () => {});

  client1.write('GET / HTTP/1.1');
});
