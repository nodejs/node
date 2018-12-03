'use strict';
const common = require('../common');

// This test should fail because at present `cluster` does not know how to share
// a socket when `worker1` binds with `port: 0`, and others try to bind to the
// assigned port number from `worker1`
//
// *Note*: since this is a `known_issue` we try to swallow all errors except
// the one we are interested in

const assert = require('assert');
const cluster = require('cluster');
const dgram = require('dgram');
const BYE = 'bye';

if (cluster.isMaster) {
  const worker1 = cluster.fork();

  // verify that Windows doesn't support this scenario
  worker1.on('error', (err) => {
    if (err.code === 'ENOTSUP') throw err;
  });

  worker1.on('message', (msg) => {
    if (typeof msg !== 'object') process.exit(0);
    if (msg.message !== 'success') process.exit(0);
    if (typeof msg.port1 !== 'number') process.exit(0);

    const worker2 = cluster.fork({ PRT1: msg.port1 });
    worker2.on('message', () => process.exit(0));
    worker2.on('exit', (code, signal) => {
      // this is the droid we are looking for
      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
    });

    // cleanup anyway
    process.on('exit', () => {
      worker1.send(BYE);
      worker2.send(BYE);
    });
  });
  // end master code
} else {
  // worker code
  process.on('message', (msg) => msg === BYE && process.exit(0));

  // First worker will bind to '0', second will try the assigned port and fail
  const PRT1 = process.env.PRT1 || 0;
  const socket1 = dgram.createSocket('udp4', () => {});
  socket1.on('error', PRT1 === 0 ? () => {} : assert.fail);
  socket1.bind(
    { address: common.localhostIPv4, port: PRT1, exclusive: false },
    () => process.send({ message: 'success', port1: socket1.address().port })
  );
}
