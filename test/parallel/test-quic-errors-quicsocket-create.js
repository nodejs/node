// Flags: --no-warnings
'use strict';

// Test QuicSocket constructor option errors

const common = require('../common');
const async_hooks = require('async_hooks');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');

// Hook verifies that no QuicSocket handles are actually created.
async_hooks.createHook({
  init: common.mustCallAtLeast((_, type) => {
    assert.notStrictEqual(type, 'QUICSOCKET');
  })
}).enable();

const { createQuicSocket } = require('net');

// Test invalid QuicSocket options argument
[1, 'test', false, 1n, null].forEach((i) => {
  assert.throws(() => createQuicSocket(i), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket port argument option
[-1, 'test', 1n, {}, [], NaN, false].forEach((port) => {
  assert.throws(() => createQuicSocket({ endpoint: { port } }), {
    code: 'ERR_SOCKET_BAD_PORT'
  });
});

// Test invalid QuicSocket addressargument option
[-1, 10, 1n, {}, [], NaN, false].forEach((address) => {
  assert.throws(() => createQuicSocket({ endpoint: { address } }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket type argument option
[1, false, 1n, {}, null, NaN].forEach((type) => {
  assert.throws(() => createQuicSocket({ endpoint: { type } }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket reuseAddr argument option
[1, NaN, 1n, null, {}, []].forEach((reuseAddr) => {
  assert.throws(() => createQuicSocket({ endpoint: { reuseAddr } }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket lookup argument option
[1, 1n, {}, [], 'test', true].forEach((lookup) => {
  assert.throws(() => createQuicSocket({ lookup }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket validateAddress argument option
[1, NaN, 1n, null, {}, []].forEach((validateAddress) => {
  assert.throws(() => createQuicSocket({ validateAddress }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket qlog argument option
[1, NaN, 1n, null, {}, []].forEach((qlog) => {
  assert.throws(() => createQuicSocket({ qlog }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});


// Test invalid QuicSocket retryTokenTimeout option
[0, 61, NaN].forEach((retryTokenTimeout) => {
  assert.throws(() => createQuicSocket({ retryTokenTimeout }), {
    code: 'ERR_OUT_OF_RANGE'
  });
});

// Test invalid QuicSocket retryTokenTimeout option
['test', null, 1n, {}, [], false].forEach((retryTokenTimeout) => {
  assert.throws(() => createQuicSocket({ retryTokenTimeout }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket maxConnectionsPerHost option
[0, Number.MAX_SAFE_INTEGER + 1, NaN].forEach((maxConnectionsPerHost) => {
  assert.throws(() => createQuicSocket({ maxConnectionsPerHost }), {
    code: 'ERR_OUT_OF_RANGE'
  });
});

// Test invalid QuicSocket maxConnectionsPerHost option
[
  'test',
  null,
  1n,
  {},
  [],
  false
].forEach((maxConnectionsPerHost) => {
  assert.throws(() => createQuicSocket({ maxConnectionsPerHost }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket maxConnections option
[0, Number.MAX_SAFE_INTEGER + 1, NaN].forEach((maxConnections) => {
  assert.throws(() => createQuicSocket({ maxConnections }), {
    code: 'ERR_OUT_OF_RANGE'
  });
});

// Test invalid QuicSocket maxConnectionsPerHost option
[
  'test',
  null,
  1n,
  {},
  [],
  false
].forEach((maxConnections) => {
  assert.throws(() => createQuicSocket({ maxConnections }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket maxStatelessResetsPerHost option
[0, Number.MAX_SAFE_INTEGER + 1, NaN].forEach((maxStatelessResetsPerHost) => {
  assert.throws(() => createQuicSocket({ maxStatelessResetsPerHost }), {
    code: 'ERR_OUT_OF_RANGE'
  });
});

// Test invalid QuicSocket maxStatelessResetsPerHost option
[
  'test',
  null,
  1n,
  {},
  [],
  false
].forEach((maxStatelessResetsPerHost) => {
  assert.throws(() => createQuicSocket({ maxStatelessResetsPerHost }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

[1, 1n, false, 'test'].forEach((options) => {
  assert.throws(() => createQuicSocket({ endpoint: options }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => createQuicSocket({ client: options }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => createQuicSocket({ server: options }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});
