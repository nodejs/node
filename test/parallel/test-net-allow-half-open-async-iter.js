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

  const clientSocket = await new Promise((resolve) => {
    const socket = net.createConnection({
      allowHalfOpen: true,
      port: server.address().port,
      host: server.address().address,
    }, common.mustCall(() => {
      resolve(socket);
    }));
    socket.on('error', common.mustNotCall());
  });

  const serverSocket = await serverSocketPromise;
  serverSocket.on('error', common.mustNotCall());

  await new Promise((resolve, reject) => {
    clientSocket.write('data written to client socket', (err) => {
      if (err) reject(err);
      else resolve();
    });
  });

  await new Promise((resolve) => {
    clientSocket.end(resolve);
  });

  let serverRead = '';
  for await (const chunk of serverSocket) {
    serverRead += chunk;
  }

  assert.strictEqual(serverRead, 'data written to client socket');
  assert.strictEqual(serverSocket.destroyed, false);

  await new Promise((resolve, reject) => {
    serverSocket.write('data written to server socket', (err) => {
      if (err) reject(err);
      else resolve();
    });
  });

  await new Promise((resolve) => {
    serverSocket.end(resolve);
  });

  let clientRead = '';
  for await (const chunk of clientSocket) {
    clientRead += chunk;
  }

  assert.strictEqual(clientRead, 'data written to server socket');

  await new Promise((resolve) => {
    server.close(resolve);
  });
})().then(common.mustCall());
