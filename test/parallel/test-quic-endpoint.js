// Flags: --no-warnings
'use strict';

const common = require('../common');

if (!common.hasQuic)
  common.skip('quic support is not enabled');

const assert = require('assert');

const {
  inspect,
} = require('util');

const {
  Endpoint,
  EndpointStats,
} = require('net/quic');

{
  const endpoint = new Endpoint();
  assert('listen' in endpoint);
  assert('connect' in endpoint);
  assert('closed' in endpoint);
  assert('close' in endpoint);
  assert('closing' in endpoint);
  assert('destroy' in endpoint);
  assert('address' in endpoint);
  assert('listening' in endpoint);
  assert('stats' in endpoint);
  assert.match(inspect(endpoint), /Endpoint {/);
  assert.strictEqual(endpoint.address, undefined);
  assert(endpoint.stats instanceof EndpointStats);

  assert(typeof endpoint.stats.createdAt, 'bignum');
  assert(typeof endpoint.stats.duration, 'bignum');
  assert(typeof endpoint.stats.bytesReceived, 'bignum');
  assert(typeof endpoint.stats.bytesSent, 'bignum');
  assert(typeof endpoint.stats.packetsReceived, 'bignum');
  assert(typeof endpoint.stats.packetsSent, 'bignum');
  assert(typeof endpoint.stats.serverSessions, 'bignum');
  assert(typeof endpoint.stats.clientSessions, 'bignum');
  assert(typeof endpoint.stats.statelessResetCount, 'bignum');
  assert(typeof endpoint.stats.serverBusyCount, 'bignum');

  assert(!endpoint.listening);
  assert(!endpoint.closing);
  endpoint.closed.then(common.mustCall());
  endpoint.close().then(common.mustCall());

  assert(endpoint.stats instanceof EndpointStats);

  const jsonStats = JSON.parse(JSON.stringify(endpoint.stats));
  assert('createdAt' in jsonStats);
  assert('duration' in jsonStats);
  assert('bytesReceived' in jsonStats);
  assert('bytesSent' in jsonStats);
  assert('packetsReceived' in jsonStats);
  assert('packetsSent' in jsonStats);
  assert('serverSessions' in jsonStats);
  assert('clientSessions' in jsonStats);
  assert('statelessResetCount' in jsonStats);
  assert('serverBusyCount' in jsonStats);

  assert(typeof jsonStats.createdAt, 'number');
  assert(typeof jsonStats.duration, 'number');
  assert(typeof jsonStats.bytesReceived, 'number');
  assert(typeof jsonStats.bytesSent, 'number');
  assert(typeof jsonStats.packetsReceived, 'number');
  assert(typeof jsonStats.packetsSent, 'number');
  assert(typeof jsonStats.serverSessions, 'number');
  assert(typeof jsonStats.clientSessions, 'number');
  assert(typeof jsonStats.statelessResetCount, 'number');
  assert(typeof jsonStats.serverBusyCount, 'number');
}

assert.throws(() => new EndpointStats(), {
  code: 'ERR_ILLEGAL_CONSTRUCTOR',
});

{
  // Just works
  const endpoint = new Endpoint();
  endpoint.listen();
  assert(endpoint.listening);
  assert(endpoint.address.address, '127.0.0.1');
  endpoint.close().then(common.mustCall());
}

{
  // Trying to bind to an invalid IP address
  const endpoint = new Endpoint({ address: { address: '192.0.2.1' } });
  endpoint.listen();
  assert(endpoint.listening);
  endpoint.closed.catch(common.mustCall((error) => {
    assert(!endpoint.listening);
    assert.strictEqual(error.code, 'ERR_QUIC_ENDPOINT_LISTEN_FAILURE');
  }));
}

{
  // Trying to bind to an already bound port
  const endpoint = new Endpoint();
  endpoint.listen();
  const endpoint2 = new Endpoint({ address: endpoint.address });
  endpoint2.listen();
  endpoint2.closed.catch(common.mustCall((error) => {
    assert(!endpoint.listening);
    assert.strictEqual(error.code, 'ERR_QUIC_ENDPOINT_LISTEN_FAILURE');
  }));
  endpoint.close();
}
