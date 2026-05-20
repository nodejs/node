import * as common from '../common/index.mjs';
import * as assert from 'assert';
import * as http from 'http';
import * as net from 'net';

const upgradeReceivedResolvers = Promise.withResolvers();
const mainBodyReceivedResolvers = Promise.withResolvers();

const EXPECTED_BODY_LENGTH = 100_003;

const server = http.createServer();
server.on('request', common.mustNotCall());
server.on('upgrade', common.mustCall(function(req, socket, upgradeHead) {
  upgradeReceivedResolvers.resolve();

  // Confirm the upgrade:
  socket.write('HTTP/1.1 101 Switching Protocols\r\n' +
              'Upgrade: custom-protocol\r\n' +
              'Connection: Upgrade\r\n' +
              '\r\n');

  // Read and check the full body is eventually received:
  let reqBodyLength = 0;
  req.on('data', (chunk) => {
    reqBodyLength += chunk.length;
    if (reqBodyLength >= 100_000) mainBodyReceivedResolvers.resolve();
  });
  req.on('end', common.mustCall(() => {
    assert.strictEqual(reqBodyLength, EXPECTED_BODY_LENGTH);
  }));

  // Head should never be available, because body is still streaming:
  assert.strictEqual(upgradeHead.length, 0);

  let socketData = Buffer.alloc(0);

  // Start streaming the raw upgrade stream immediately as well, and check we do
  // eventually get the complete stream:
  socket.on('data', (d) => {
    socketData = Buffer.concat([socketData, d]);
  });

  socket.on('end', common.mustCall(() => {
    assert.strictEqual(socketData.toString(), 'upgrade head\npost-upgrade message');
    socket.end();
  }));
}));

await new Promise((resolve) => server.listen(0, () => resolve()));

const conn = net.createConnection(server.address().port);
conn.setEncoding('utf8');

await new Promise((resolve) => conn.on('connect', resolve));

// Write request headers, leave the body pending:
conn.write(
  'POST / HTTP/1.1\r\n' +
  'host: localhost\r\n' +
  'Upgrade: custom-protocol\r\n' +
  'Connection: Upgrade\r\n' +
  'transfer-encoding: chunked\r\n' +
  '\r\n'
);

// Make sure the server has processed the above & fired 'upgrade' before the body
// data starts streaming:
await upgradeReceivedResolvers.promise;

// Write a 200KB body in small chunks:
for (let i = 0; i < 20_000; i++) {
  conn.write('5\r\nhello\r\n'); // 10 byte chunk = 5 byte body
}

// Wait until all that has been processed, to strongly encourage (though we can't guarantee)
// the next chunk to arrive in one go, and have to be processed & correctly split
await mainBodyReceivedResolvers.promise;

// Final chunk + end-body + 'upgrade head' upgrade stream:
conn.write(
  '3\r\nbye\r\n' +
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
