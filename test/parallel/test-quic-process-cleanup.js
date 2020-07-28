// Flags: --no-warnings --expose-internals
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Test that shutting down a process containing an active QUIC server behaves
// well. We use Workers because they have a more clearly defined shutdown
// sequence and we can stop execution at any point.

const { kRemoveFromSocket } = require('internal/quic/core');
const { createQuicSocket } = require('net');
const { Worker, workerData } = require('worker_threads');

if (workerData == null) {
  new Worker(__filename, { workerData: { removeFromSocket: true } });
  new Worker(__filename, { workerData: { removeFromSocket: false } });
  return;
}

const { key, cert, ca } = require('../common/quic');
const options = { key, cert, ca, alpn: 'meow' };

const client = createQuicSocket({ client: options });
const server = createQuicSocket({ server: options });
server.on('close', common.mustNotCall());
client.on('close', common.mustNotCall());

(async function() {
  server.on('session', common.mustCall(async (session) => {
    const stream = await session.openStream({ halfOpen: false });
    stream.write('Hi!');
    stream.on('data', common.mustNotCall());
    stream.on('finish', common.mustNotCall());
    stream.on('close', common.mustNotCall());
    stream.on('end', common.mustNotCall());

    session.on('close', common.mustNotCall());
  }));

  await server.listen();

  const req = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port
  });

  req.on('stream', common.mustCall(() => {
    if (workerData.removeFromSocket)
      req[kRemoveFromSocket]();
    process.exit();  // Exits the worker thread
  }));

  req.on('close', common.mustNotCall());
})().then(common.mustCall());
