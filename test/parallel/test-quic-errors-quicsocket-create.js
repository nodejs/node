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

const { createSocket } = require('quic');

// Test invalid QuicSocket options argument
[1, 'test', false, 1n, null].forEach((i) => {
  assert.throws(() => createSocket(i), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket port argument option
[-1, 'test', 1n, {}, [], NaN, false].forEach((port) => {
  assert.throws(() => createSocket({ endpoint: { port } }), {
    code: 'ERR_SOCKET_BAD_PORT'
  });
});

// Test invalid QuicSocket addressargument option
[-1, 10, 1n, {}, [], NaN, false].forEach((address) => {
  assert.throws(() => createSocket({ endpoint: { address } }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket type argument option
[1, false, 1n, {}, null, NaN].forEach((type) => {
  assert.throws(() => createSocket({ endpoint: { type } }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket ipv6Only argument option
[1, NaN, 1n, null, {}, []].forEach((ipv6Only) => {
  assert.throws(() => createSocket({ endpoint: { ipv6Only } }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket reuseAddr argument option
[1, NaN, 1n, null, {}, []].forEach((reuseAddr) => {
  assert.throws(() => createSocket({ endpoint: { reuseAddr } }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket lookup argument option
[1, 1n, {}, [], 'test', true].forEach((lookup) => {
  assert.throws(() => createSocket({ lookup }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket validateAddress argument option
[1, NaN, 1n, null, {}, []].forEach((validateAddress) => {
  assert.throws(() => createSocket({ validateAddress }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket validateAddressLRU argument option
[1, NaN, 1n, null, {}, []].forEach((validateAddressLRU) => {
  assert.throws(() => createSocket({ validateAddressLRU }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket autoClose argument option
[1, NaN, 1n, null, {}, []].forEach((autoClose) => {
  assert.throws(() => createSocket({ autoClose }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket qlog argument option
[1, NaN, 1n, null, {}, []].forEach((qlog) => {
  assert.throws(() => createSocket({ qlog }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});


// Test invalid QuicSocket retryTokenTimeout option
[0, 61, NaN].forEach((retryTokenTimeout) => {
  assert.throws(() => createSocket({ retryTokenTimeout }), {
    code: 'ERR_OUT_OF_RANGE'
  });
});

// Test invalid QuicSocket retryTokenTimeout option
['test', null, 1n, {}, [], false].forEach((retryTokenTimeout) => {
  assert.throws(() => createSocket({ retryTokenTimeout }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket maxConnectionsPerHost option
[0, Number.MAX_SAFE_INTEGER + 1, NaN].forEach((maxConnectionsPerHost) => {
  assert.throws(() => createSocket({ maxConnectionsPerHost }), {
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
  assert.throws(() => createSocket({ maxConnectionsPerHost }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket maxConnections option
[0, Number.MAX_SAFE_INTEGER + 1, NaN].forEach((maxConnections) => {
  assert.throws(() => createSocket({ maxConnections }), {
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
  assert.throws(() => createSocket({ maxConnections }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket maxStatelessResetsPerHost option
[0, Number.MAX_SAFE_INTEGER + 1, NaN].forEach((maxStatelessResetsPerHost) => {
  assert.throws(() => createSocket({ maxStatelessResetsPerHost }), {
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
  assert.throws(() => createSocket({ maxStatelessResetsPerHost }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

[1, 1n, false, 'test'].forEach((options) => {
  assert.throws(() => createSocket({ endpoint: options }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => createSocket({ client: options }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => createSocket({ server: options }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});
