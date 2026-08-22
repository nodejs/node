'use strict';

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (cluster.isPrimary) {
  cluster.fork().on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
  return;
}

function listen() {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once('error', reject);
    server.once('listening', () => resolve(server));
    server.listen({ host: '127.0.0.1', port: common.PORT });
  });
}

(async () => {
  const server1 = await listen();
  await assert.rejects(listen(), { code: 'EADDRINUSE' });
  await new Promise((resolve) => server1.close(resolve));
  cluster.worker.disconnect();
})().then(common.mustCall());
