'use strict';

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (process.argv[2] === undefined) {
  const { fork } = require('child_process');

  function run(policy) {
    return new Promise((resolve) => {
      const child = fork(__filename, [policy]);
      child.on('exit', common.mustCall((code) => {
        assert.strictEqual(code, 0);
        resolve();
      }));
    });
  }

  (async () => {
    await run('none');
    await run('rr');
  })().then(common.mustCall());
  return;
}

cluster.schedulingPolicy =
  process.argv[2] === 'rr' ? cluster.SCHED_RR : cluster.SCHED_NONE;

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
