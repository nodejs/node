'use strict';

// Tests QuicClientSession constructor options errors

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { createSocket } = require('quic');

const client = createSocket();

// Test invalid minDHSize options argument
['test', 1n, {}, [], false].forEach((minDHSize) => {
  assert.throws(() => client.connect({ minDHSize }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid port argument option
[-1, 'test', 1n, {}, [], NaN, false].forEach((port) => {
  assert.throws(() => client.connect({ port }), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
});

// Test invalid address argument option
[-1, 10, 1n, {}, []].forEach((address) => {
  assert.throws(() => client.connect({ address }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test address can't be IP address argument option
[
  '0.0.0.0',
  '8.8.8.8',
  '127.0.0.1',
  '192.168.0.1',
  '::',
  '1::',
  '::1',
  '1::8',
  '1::7:8',
  '1:2:3:4:5:6:7:8',
  '1:2:3:4:5:6::8',
  '2001:0000:1234:0000:0000:C1C0:ABCD:0876',
  '3ffe:0b00:0000:0000:0001:0000:0000:000a',
  'a:0:0:0:0:0:0:0',
  'fe80::7:8%eth0',
  'fe80::7:8%1'
].forEach((address) => {
  assert.throws(() => client.connect({ address }), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
});

// Test invalid remoteTransportParams argument option
[-1, 'test', 1n, {}, []].forEach((remoteTransportParams) => {
  assert.throws(() => client.connect({ remoteTransportParams }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid sessionTicket argument option
[-1, 'test', 1n, {}, []].forEach((sessionTicket) => {
  assert.throws(() => client.connect({ sessionTicket }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

// Test invalid alpn argument option
[-1, 10, 1n, {}, []].forEach((alpn) => {
  assert.throws(() => client.connect({ alpn }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});
