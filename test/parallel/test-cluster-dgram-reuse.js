'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const dgram = require('dgram');

if (common.isWindows) {
  common.skip('dgram clustering is currently not supported ' +
              'on windows.');
  return;
}

if (cluster.isMaster) {
  cluster.fork().on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
  return;
}

const sockets = [];
function next() {
  sockets.push(this);
  if (sockets.length !== 2)
    return;

  // Work around health check issue
  process.nextTick(() => {
    for (let i = 0; i < sockets.length; i++)
      sockets[i].close(close);
  });
}

let waiting = 2;
function close() {
  if (--waiting === 0)
    cluster.worker.disconnect();
}

for (let i = 0; i < 2; i++)
  dgram.createSocket({ type: 'udp4', reuseAddr: true }).bind(0, next);
