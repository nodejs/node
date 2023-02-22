'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// This test sends an invalid character to a HTTP server and purposely
// does not handle clientError (even if it sets an event handler).
//
// The idea is to let the server emit multiple errors on the socket,
// mostly due to parsing error, and make sure they don't result
// in leaking event listeners.

{
  let i = 0;
  let socket;
  const mustNotCall = common.mustNotCall.bind(null);
  process.on('warning', mustNotCall);

  const server = http.createServer(common.mustNotCall());

  server.on('clientError', common.mustCallAtLeast((err) => {
    assert.strictEqual(err.code, 'HPE_INVALID_METHOD');
    assert.strictEqual(err.rawPacket.toString(), '*');

    if (i === 20) {
      socket.end();
    } else {
      socket.write('*');
      i++;
    }
  }, 1));

  server.listen(0, () => {
    socket = net.createConnection({ port: server.address().port });

    socket.on('connect', () => {
      socket.write('*');
    });

    socket.on('close', () => {
      server.close();
      process.removeListener('warning', mustNotCall);
    });
  });
}

{
  const valid = 'GET / HTTP/1.1\r\nHost:a\r\nContent-Length:1\r\n\r\n1\r\n';

  const server = http.createServer((req, res) => {
    const handler = common.mustCall(function handler(error) {
      req.socket.removeListener('error', handler);
    }, 2);

    req.on('end', handler);
    req.socket.on('error', handler);

    res.removeHeader('date');
    res.writeHead(204);
    res.end();
  });

  server.listen(0, () => {
    const client = net.createConnection({ host: '127.0.0.1', port: server.address().port });
    let data = Buffer.alloc(0);

    client.on('data', (chunk) => {
      data = Buffer.concat([data, chunk]);
    });

    client.on('end', () => {
      assert.strictEqual(
        data.toString('utf-8'),
        'HTTP/1.1 204 No Content\r\nConnection: keep-alive\r\nKeep-Alive: timeout=5\r\n\r\n' +
        'HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n',
      );
      server.close();
    });

    client.write(valid + 'INVALID');
  });
}
