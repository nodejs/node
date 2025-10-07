'use strict';

const common = require('../common');
const cluster = require('cluster');
const assert = require('assert');

const acts = {
  WORKER1_SERVER1_CLOSED: { cmd: 'WORKER1_SERVER1_CLOSED' },
  WORKER2_SERVER1_STARTED: { cmd: 'WORKER2_SERVER1_STARTED' },
  WORKER1_SERVER2_CLOSED: { cmd: 'WORKER1_SERVER2_CLOSED' },
};

if (cluster.isMaster) {
  const currentHost = '::';
  const worker1 = cluster.fork({
    WORKER_ID: 'worker1',
    HOST: currentHost,
  });
  let worker2;
  worker1.on('error', common.mustNotCall());
  worker1.on('message', onMessage);

  function createWorker2() {
    worker2 = cluster.fork({
      WORKER_ID: 'worker2',
      HOST: currentHost,
    });
    worker2.on('error', common.mustNotCall());
    worker2.on('message', onMessage);
  }

  function onMessage(msg) {
    switch (msg.cmd) {
      case acts.WORKER1_SERVER1_CLOSED.cmd:
        createWorker2();
        break;
      case acts.WORKER2_SERVER1_STARTED.cmd:
        worker1.send(acts.WORKER2_SERVER1_STARTED);
        break;
      case acts.WORKER1_SERVER2_CLOSED.cmd:
        worker1.kill();
        worker2.kill();
        break;
      default:
        assert.fail(`Unexpected message ${msg.cmd}`);
    }
  }
} else {
  const WORKER_ID = process.env.WORKER_ID;
  function createServer() {
    return new Promise((resolve, reject) => {
      const net = require('net');
      const PORT = 8000;
      const server = net
        .createServer((socket) => {
          socket.end(
            `Handled by worker ${process.env.WORKER_ID} (${process.pid})\n`
          );
        })
        .on('error', (e) => {
          reject(e);
        });

      server.listen(
        {
          port: PORT,
          host: process.env.HOST,
        },
        () => resolve(server)
      );
    });
  }
  (async () => {
    const server1 = await createServer();
    if (WORKER_ID === 'worker2') {
      process.send(acts.WORKER2_SERVER1_STARTED);
    } else {
      await createServer().catch(common.mustCall());
      await new Promise((r) => server1.close(r));
      process.send(acts.WORKER1_SERVER1_CLOSED);

      process.on('message', async (msg) => {
        if (msg.cmd === acts.WORKER2_SERVER1_STARTED.cmd) {
          const server2 = await createServer();
          await new Promise((r) => server2.close(r));
          process.send(acts.WORKER1_SERVER2_CLOSED);
        } else {
          assert.fail(`Unexpected message ${msg.cmd}`);
        }
      });
    }
  })().then(common.mustCall());
}
