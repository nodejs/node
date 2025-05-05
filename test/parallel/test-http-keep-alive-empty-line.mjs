import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { createServer } from 'node:http';
import { connect } from 'node:net';

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

server.listen(0);

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

  setTimeout(() => {
    client.write('\r\n');
  }, 100);

  client.on('data', (data) => {
    const status = data.toString().split(' ')[1];
    assert.strictEqual(status, '404');
  });
  client.on('end', common.mustCall());
});
