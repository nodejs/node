// Flags: --no-warnings
'use strict';

const common = require('../common');

if (!common.hasQuic)
  common.skip('missing quic');

const { createQuicSocket, BlockList } = require('net');
const assert = require('assert');

const { key, cert, ca } = require('../common/quic');
const { once } = require('events');

const idleTimeout = common.platformTimeout(1);
const options = { key, cert, ca, alpn: 'zzz', idleTimeout };

const client = createQuicSocket({ client: options });
const server = createQuicSocket({ server: options });

assert(client.blockList instanceof BlockList);
assert(server.blockList instanceof BlockList);

client.blockList.addAddress('10.0.0.1');

assert(client.blockList.check('10.0.0.1'));

// Connection fails because the IP address is blocked
assert.rejects(client.connect({ address: '10.0.0.1' }), {
  code: 'ERR_OPERATION_FAILED'
}).then(common.mustCall());

server.blockList.addAddress('127.0.0.1');

(async () => {
  server.on('session', common.mustNotCall());

  await server.listen();

  const session = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
    idleTimeout,
  });

  session.on('secure', common.mustNotCall());

  await once(session, 'close');

  await Promise.all([server.close(), client.close()]);

})().then(common.mustCall());
