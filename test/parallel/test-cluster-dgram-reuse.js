'use strict';
const common = require('../common');
if (common.isWindows)
  common.skip('dgram clustering is currently not supported on windows.');

const assert = require('assert');
const cluster = require('cluster');
const dgram = require('dgram');

if (cluster.isMaster) {
  cluster.fork().on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
  return;
}

let waiting = 2;
function close() {
  if (--waiting === 0)
    cluster.worker.disconnect();
}

const options = { type: 'udp4', reuseAddr: true };
const socket1 = dgram.createSocket(options);
const socket2 = dgram.createSocket(options);

socket1.bind(0, () => {
  socket2.bind(socket1.address().port, () => {
    // Work around health check issue
    process.nextTick(() => {
      socket1.close(close);
      socket2.close(close);
    });
  });
});
