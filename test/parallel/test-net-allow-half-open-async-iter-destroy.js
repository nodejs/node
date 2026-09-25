'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

(async function() {
  let resolveServerSocket;
  const serverSocketPromise = new Promise((resolve) => {
    resolveServerSocket = resolve;
  });

  const server = net.createServer({
    allowHalfOpen: true,
  }, common.mustCall((socket) => {
    resolveServerSocket(socket);
  }));

  server.on('error', common.mustNotCall());
  server.on('close', common.mustCall());

  await new Promise((resolve) => {
    server.listen(0, common.localhostIPv4, resolve);
  });

  const clientSocket = net.createConnection({
    port: server.address().port,
    host: server.address().address,
  });
  clientSocket.on('error', common.mustNotCall());

  await new Promise((resolve) => {
    clientSocket.end('data', resolve);
  });

  const serverSocket = await serverSocketPromise;

  let serverRead = '';
  for await (const chunk of serverSocket) {
    serverRead += chunk;
  }

  const destroyed = serverSocket.destroyed;
  serverSocket.destroy();
  clientSocket.destroy();

  await new Promise((resolve) => {
    server.close(resolve);
  });

  assert.strictEqual(serverRead, 'data');
  assert.strictEqual(destroyed, true);
})().then(common.mustCall());
