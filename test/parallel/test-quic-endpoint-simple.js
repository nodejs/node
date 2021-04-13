'use strict';

const common = require('../common');

if (!common.hasQuic)
  common.skip('quic support is not enabled');

const assert = require('assert');
const {
  SocketAddress,
} = require('net');

const {
  Endpoint,
} = require('net/quic');

['abc', 1, true, null].forEach((i) => {
  assert.throws(() => new Endpoint(i), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

const server = new Endpoint();

server.closed.then(common.mustCall(() => {
  assert.strictEqual(server.address, undefined);
  assert.strictEqual(server.listening, false);
}));

assert.strictEqual(server.listening, false);

assert.strictEqual(server.address, undefined);

['abc', 1, true, null].forEach((i) => {
  assert.throws(() => server.listen(i), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

assert.strictEqual(server.listening, false);

server.listen({ alpn: 'zzz' });

// Makes sure that getter returns the same cached
// object as opposed to creating a new one.
assert.strictEqual(server.address, server.address);
assert(server.address instanceof SocketAddress);

// The port will be randomly assigned.
const {
  port,
  address,
  family,
} = server.address;
assert.notStrictEqual(port, 0);
assert.strictEqual(address, '127.0.0.1');
assert.strictEqual(family, 'ipv4');

assert.strictEqual(server.listening, true);

assert.strictEqual(server.closing, false);

const {
  createdAt,
  duration,
  bytesReceived,
  bytesSent,
  packetsReceived,
  packetsSent,
  serverSessions,
  clientSessions,
  statelessResetCount,
  serverBusyCount,
} = server.stats;

assert.strictEqual(typeof createdAt, 'bigint');
assert.strictEqual(typeof duration, 'bigint');
assert.strictEqual(typeof bytesReceived, 'bigint');
assert.strictEqual(typeof bytesSent, 'bigint');
assert.strictEqual(typeof packetsReceived, 'bigint');
assert.strictEqual(typeof packetsSent, 'bigint');
assert.strictEqual(typeof serverSessions, 'bigint');
assert.strictEqual(typeof clientSessions, 'bigint');
assert.strictEqual(typeof statelessResetCount, 'bigint');
assert.strictEqual(typeof serverBusyCount, 'bigint');

server.close().then(common.mustCall());
