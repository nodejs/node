import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { createServer } from 'node:http';
import { connect } from 'node:net';

// This test ensures that data like an empty line (`\r\n`) recevied by the
// server after a request, does not reset the keep-alive timeout. See
// https://github.com/nodejs/node/issues/58140.

const server = createServer({
  connectionsCheckingInterval: 100,
  headersTimeout: 100,
  keepAliveTimeout: 300
}, (req, res) => {
  res.writeHead(404);
  res.end();

  req.socket.on('close', common.mustCall(() => {
    server.close();
  }));
});

server.listen(0, () => {
  const client = connect({
    host: 'localhost',
    port: server.address().port,
  }, () => {
    client.write(
      'GET / HTTP/1.1\r\n' +
          'Host: localhost:3000\r\n' +
          'Content-Length: 0\r\n' +
          '\r\n'
    );

    let response = '';
    let responseReceived = false;

    client.setEncoding('utf-8');
    client.on('data', (chunk) => {
      response += chunk;

      // Check if we've received the full header (ending with \r\n\r\n)
      if (response.includes('\r\n\r\n')) {
        responseReceived = true;
        const statusLine = response.split('\r\n')[0];
        const status = statusLine.split(' ')[1];
        assert.strictEqual(status, '404');
        client.write('\r\n');
      }
    });
    client.on('end', common.mustCall(() => {
      assert.ok(responseReceived);
    }));
  });
});
