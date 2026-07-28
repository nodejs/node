import * as common from '../common/index.mjs';
import * as assert from 'assert';
import * as http from 'http';
import * as net from 'net';

const EXPECTED_BODY_LENGTH = 12;

const server = http.createServer();
server.on('request', common.mustNotCall());
server.on('upgrade', common.mustCall((req, socket, upgradeHead) => {
  // Confirm the upgrade:
  socket.write('HTTP/1.1 101 Switching Protocols\r\n' +
              'Upgrade: custom-protocol\r\n' +
              'Connection: Upgrade\r\n' +
              '\r\n');

  // Read and validate the request body:
  let reqBodyLength = 0;
  req.on('data', (chunk) => { reqBodyLength += chunk.length; });
  req.on('end', common.mustCall(() => {
    assert.strictEqual(reqBodyLength, EXPECTED_BODY_LENGTH);

    // Defer upgrade stream read slightly to make sure it doesn't start
    // streaming along with the request body, until we actually read it:
    setTimeout(common.mustCall(() => {
      let socketData = upgradeHead;

      socket.on('data', (d) => {
        socketData = Buffer.concat([socketData, d]);
      });

      socket.on('end', common.mustCall(() => {
        assert.strictEqual(socketData.toString(), 'upgrade head\npost-upgrade message');
        socket.end();
      }));
    }), 10);
  }));
}));

await new Promise((resolve) => server.listen(0, () => resolve()));

const conn = net.createConnection(server.address().port);
conn.setEncoding('utf8');

await new Promise((resolve) => conn.on('connect', resolve));

// Write request headers, body & upgrade head all together:
conn.write(
  'POST / HTTP/1.1\r\n' +
  'host: localhost\r\n' +
  'Upgrade: custom-protocol\r\n' +
  'Connection: Upgrade\r\n' +
  'transfer-encoding: chunked\r\n' +
  '\r\n' +
  'C\r\nrequest body\r\n' + // 12 byte body sent immediately
  '0\r\n\r\n' +
  'upgrade head'
);

const response = await new Promise((resolve) => conn.once('data', resolve));
assert.ok(response.startsWith('HTTP/1.1 101 Switching Protocols\r\n'));

// Send more data after connection is confirmed:
conn.write('\npost-upgrade message');
conn.end();

await new Promise((resolve) => conn.on('end', resolve));

server.close();
