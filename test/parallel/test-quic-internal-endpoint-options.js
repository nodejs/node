// Flags: --expose-internals --no-warnings
'use strict';

require('../common');

const {
  constants,
  EndpointOptions,
} = require('internal/quic/quic');

const test = require('node:test');
const {
  SocketAddress
} = require('net');

test('Creating a simple internalBinding(\'quic\').EndpointOptions works', () => {
  const addr = new SocketAddress({});

  test('With no second arg...', () => new EndpointOptions(addr));
  test('With an empty second arg...', () => new EndpointOptions(addr, {}));
});

test('We can set some options', () => {
  const addr = new SocketAddress({});

  // Note that we cannot test passing the incorrect types here because the code will
  // assert and fail. This is testing the low-level binding. The API layer on top will
  // need to perform proper type checking.
  const options = new EndpointOptions(addr, {
    retryTokenExpiration: 123,
    tokenExpiration: 123,
    maxWindowOverride: 123,
    maxStreamWindowOverride: 123,
    maxConnectionsPerHost: 123,
    maxConnectionsTotal: 123,
    maxStatelessResets: 123,
    addressLRUSize: 123,
    retryLimit: 123,
    maxPayloadSize: 1234,
    unacknowledgedPacketThreshold: 123,
    validateAddress: true,
    disableStatelessReset: false,
    rxPacketLoss: 1.0,
    txPacketLoss: 0.0,
    ccAlgorithm: constants.CongestionControlAlgorithm.CUBIC,
    ipv6Only: false,
    receiveBufferSize: 123,
    sendBufferSize: 123,
    ttl: 99,

    // Other properties are ignored
    abcfoo: '123',
  });

  options.generateResetTokenSecret();
  // The secret must be exactly 16 bytes long
  options.setResetTokenSecret(Buffer.from('hellotherequic!!'));
});
