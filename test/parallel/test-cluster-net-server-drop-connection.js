'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const cluster = require('cluster');
const tmpdir = require('../common/tmpdir');

// The core has bug in handling pipe handle by ipc when platform is win32,
// it can be triggered on win32. I will fix it in another pr.
if (common.isWindows)
  common.skip('no setSimultaneousAccepts on pipe handle');

let connectionCount = 0;
let listenCount = 0;
let worker1;
let worker2;

function request(path) {
  for (let i = 0; i < 10; i++) {
    net.connect(path);
  }
}

function handleMessage(message) {
  assert.match(message.action, /listen|connection/);
  if (message.action === 'listen') {
    if (++listenCount === 2) {
      request(common.PIPE);
    }
  } else if (message.action === 'connection') {
    if (++connectionCount === 10) {
      worker1.send({ action: 'disconnect' });
      worker2.send({ action: 'disconnect' });
    }
  }
}

if (cluster.isPrimary) {
  cluster.schedulingPolicy = cluster.SCHED_RR;
  tmpdir.refresh();
  worker1 = cluster.fork({ maxConnections: 1, pipePath: common.PIPE });
  worker2 = cluster.fork({ maxConnections: 9, pipePath: common.PIPE });
  worker1.on('message', common.mustCall((message) => {
    handleMessage(message);
  }, 2));
  worker2.on('message', common.mustCall((message) => {
    handleMessage(message);
  }, 10));
} else {
  const server = net.createServer(common.mustCall((socket) => {
    process.send({ action: 'connection' });
  }, +process.env.maxConnections));

  server.listen(process.env.pipePath, common.mustCall(() => {
    process.send({ action: 'listen' });
  }));

  server.maxConnections = +process.env.maxConnections;

  process.on('message', common.mustCall((message) => {
    assert.strictEqual(message.action, 'disconnect');
    process.disconnect();
  }));
}
