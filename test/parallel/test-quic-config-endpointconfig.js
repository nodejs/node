'use strict';

const common = require('../common');

if (!common.hasQuic)
  common.skip('quic support is not enabled');

const assert = require('assert');
const {
  SocketAddress,
} = require('net');

const {
  inspect,
} = require('util');

const {
  EndpointConfig,
} = require('net/quic');

{
  // Works... no errors thrown
  const ec = new EndpointConfig();
  assert(EndpointConfig.isEndpointConfig(ec));
  assert(!EndpointConfig.isEndpointConfig({}));
  assert.match(inspect(ec), /EndpointConfig {/);
}

{
  const ec = new EndpointConfig({});
  assert(EndpointConfig.isEndpointConfig(ec));
}

['abc', 1, true, null].forEach((i) => {
  assert.throws(() => new EndpointConfig(i), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

['abc', 1, false, null].forEach((address) => {
  assert.throws(() => new EndpointConfig({ address }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

{
  new EndpointConfig({ address: { } });

  new EndpointConfig({
    address: {
      address: '123.123.123.0'
    },
  });

  new EndpointConfig({
    address: {
      address: '123.123.123.0',
      port: 123,
    },
  });

  new EndpointConfig({
    address: {
      address: '::1',
      port: 123,
      family: 'ipv6',
    },
  });

  new EndpointConfig({ address: new SocketAddress() });

  new EndpointConfig({
    address: {
      address: '123.123.123.0'
    },
  });

  new EndpointConfig({
    address: {
      address: '123.123.123.0',
      port: 123,
    },
  });

  new EndpointConfig({
    address: new SocketAddress({
      address: '::1',
      port: 123,
      family: 'ipv6',
    }),
  });
}

[
  'retryTokenExpiration',
  'tokenExpiration',
  'maxWindowOverride',
  'maxStreamWindowOverride',
  'maxConnectionsPerHost',
  'maxConnectionsTotal',
  'maxStatelessResets',
  'addressLRUSize',
  'retryLimit',
  'maxPayloadSize',
  'unacknowledgedPacketThreshold',
].forEach((i) => {
  ['abc', true, {}, [], null, 1.1, Infinity].forEach((n) => {
    assert.throws(() => new EndpointConfig({ [i]: n }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });
});

[
  'validateAddress',
  'disableStatelessReset',
].forEach((i) => {
  ['abc', 1, {}, [], null, 1.1, Infinity].forEach((n) => {
    assert.throws(() => new EndpointConfig({ [i]: n }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });
});

[
  'rxPacketLoss',
  'txPacketLoss',
].forEach((i) => {
  ['abc', {}, [], null].forEach((n) => {
    assert.throws(() => new EndpointConfig({ [i]: n }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });
});

[
  'rxPacketLoss',
  'txPacketLoss',
].forEach((i) => {
  [-1, 1.1].forEach((n) => {
    assert.throws(() => new EndpointConfig({ [i]: n }), {
      code: 'ERR_OUT_OF_RANGE',
    });
  });
});

[1, true, {}, [], null].forEach((ccAlgorithm) => {
  assert.throws(() => new EndpointConfig({ ccAlgorithm }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

{
  new EndpointConfig({ ccAlgorithm: 'cubic' });
  new EndpointConfig({ ccAlgorithm: 'reno' });
  assert.throws(() => new EndpointConfig({ ccAlgorithm: 'foo' }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

['abc', 1, true, null].forEach((udp) => {
  assert.throws(() => new EndpointConfig({ udp }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

{
  new EndpointConfig({ udp: { } });
}

['abc', {}, [], null].forEach((i) => {
  assert.throws(() => new EndpointConfig({ udp: {
    ipv6Only: i
  } }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

['abc', {}, [], null].forEach((i) => {
  assert.throws(() => new EndpointConfig({ udp: {
    sendBufferSize: i
  } }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

['abc', {}, [], null].forEach((i) => {
  assert.throws(() => new EndpointConfig({ udp: {
    ttl: i
  } }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

['abc', 1, {}, []].forEach((resetTokenSecret) => {
  assert.throws(() => new EndpointConfig({ resetTokenSecret }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

assert.throws(() => new EndpointConfig({
  resetTokenSecret: Buffer.alloc(4)
}), {
  code: 'ERR_INVALID_ARG_VALUE',
});

assert.throws(() => new EndpointConfig({
  resetTokenSecret: Buffer.alloc(17)
}), {
  code: 'ERR_INVALID_ARG_VALUE',
});

[
  Buffer.alloc(16),
  new Int8Array(16),
  new Int16Array(8),
  new Int32Array(4),
  new BigInt64Array(2),
  new Uint8Array(16),
  new Uint16Array(8),
  new Uint32Array(4),
  new BigUint64Array(2),
  new ArrayBuffer(16),
  new DataView(new ArrayBuffer(16)),
].forEach((resetTokenSecret) => {
  new EndpointConfig({ resetTokenSecret });
});
