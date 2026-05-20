import * as common from '../common/index.mjs';
import * as assert from 'assert';
import * as http from 'http';
import * as net from 'net';

const upgradeReceivedResolvers = Promise.withResolvers();

const server = http.createServer();
server.on('request', common.mustNotCall());
server.on('upgrade', function(req, socket, upgradeHead) {
  upgradeReceivedResolvers.resolve();

  // As soon as the body starts arriving, simulate an error
  req.on('data', () => {
    req.socket.destroy(new Error('simulated body error'));
  });
});

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

conn.write('5\r\nhello\r\n');

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, 'simulated body error');
  conn.destroy();
  server.close();
}));
