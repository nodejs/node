'use strict';

// Test QuicSocket constructor option errors

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');

const { createSocket } = require('quic');

// Test invalid QuicSocket options argument
[1, 'test', false, 1n, null].forEach((i) => {
  assert.throws(() => createSocket(i), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket port argument option
[-1, 'test', 1n, {}, [], NaN, false].forEach((port) => {
  assert.throws(() => createSocket({ port }), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
});

// Test invalid QuicSocket addressargument option
[-1, 10, 1n, {}, [], NaN, false].forEach((address) => {
  assert.throws(() => createSocket({ address }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket type argument option
[1, false, 1n, {}, null, NaN].forEach((type) => {
  assert.throws(() => createSocket({ type }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket lookup argument option
[1, false, NaN, 1n, null, {}, []].forEach((lookup) => {
  assert.throws(() => createSocket({ lookup }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket ipv6Only argument option
[1, NaN, 1n, null, {}, []].forEach((ipv6Only) => {
  assert.throws(() => createSocket({ ipv6Only }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket reuseAddr argument option
[1, NaN, 1n, null, {}, []].forEach((reuseAddr) => {
  assert.throws(() => createSocket({ reuseAddr }), {
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

// Test invalid QuicSocket retryTokenTimeout option
[0, 61].forEach((retryTokenTimeout) => {
  assert.throws(() => createSocket({ retryTokenTimeout }), {
    code: 'ERR_OUT_OF_RANGE'
  });
});

// Test invalid QuicSocket retryTokenTimeout option
['test', null, NaN, 1n, {}, [], false].forEach((retryTokenTimeout) => {
  assert.throws(() => createSocket({ retryTokenTimeout }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket maxConnectionsPerHost option
[0].forEach((maxConnectionsPerHost) => {
  assert.throws(() => createSocket({ maxConnectionsPerHost }), {
    code: 'ERR_OUT_OF_RANGE'
  });
});

// Test invalid QuicSocket maxConnectionsPerHost option
[
  Number.MAX_SAFE_INTEGER + 1,
  'test',
  null,
  NaN,
  1n,
  {},
  [],
  false
].forEach((maxConnectionsPerHost) => {
  assert.throws(() => createSocket({ maxConnectionsPerHost }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid QuicSocket type argument option
[1, NaN, 1n, null, {}, []].forEach((type) => {
  assert.throws(() => createSocket({ type }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});
