// Flags: --expose-internals --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { makeUDPPair } = require('../common/udppair');
const assert = require('assert');
const { createQuicSocket } = require('net');
const { kUDPHandleForTesting } = require('internal/quic/core');

const { key, cert, ca } = require('../common/quic');

const { serverSide, clientSide } = makeUDPPair();
const options = { key, cert, ca, alpn: 'meow' };

const server = createQuicSocket({
  validateAddress: true,
  endpoint: { [kUDPHandleForTesting]: serverSide._handle },
  server: options,
  qlog: true
});
serverSide.afterBind();

const client = createQuicSocket({
  endpoint: { [kUDPHandleForTesting]: clientSide._handle },
  client: options,
  qlog: true
});
clientSide.afterBind();

(async function() {
  server.on('session', common.mustCall(async (session) => {
    gatherQlog(session, 'server');
    (await session.openStream({ halfOpen: true })).end('Hi!');
  }));

  await server.listen();

  const req = await client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
    qlog: true
  });

  gatherQlog(req, 'client');

  req.on('stream', common.mustCall((stream) => {
    stream.resume();
    stream.on('end', common.mustCall(() => {
      req.close();
    }));
  }));
})().then(common.mustCall());

function setupQlog(qlog) {
  let data = '';
  qlog.setEncoding('utf8');
  qlog.on('data', (chunk) => data += chunk);
  qlog.once('end', common.mustCall(() => {
    const { qlog_version, traces } = JSON.parse(data);
    assert.strictEqual(typeof qlog_version, 'string');
    assert.strictEqual(typeof traces, 'object');
  }));
}

function gatherQlog(session, id) {
  switch (id) {
    case 'server':
      setupQlog(session.qlog);
      break;
    case 'client':
      session.on('qlog', setupQlog);
      break;
  }
}
