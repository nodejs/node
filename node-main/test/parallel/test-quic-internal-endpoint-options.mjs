// Flags: --experimental-quic --no-warnings
import { hasQuic, skip } from '../common/index.mjs';
import { strictEqual, throws } from 'node:assert';
import { inspect } from 'node:util';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

// Import after the hasQuic check
const { QuicEndpoint } = await import('node:quic');

// Reject invalid options
['a', null, false, NaN].forEach((i) => {
  throws(() => new QuicEndpoint(i), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

// Just Works... using all defaults
new QuicEndpoint();

// Test various option values
const cases = [
  {
    key: 'retryTokenExpiration',
    valid: [
      1, 10, 100, 1000, 10000, 10000n,
    ],
    invalid: [-1, -1n, 'a', null, false, true, {}, [], () => {}]
  },
  {
    key: 'tokenExpiration',
    valid: [
      1, 10, 100, 1000, 10000, 10000n,
    ],
    invalid: [-1, -1n, 'a', null, false, true, {}, [], () => {}]
  },
  {
    key: 'maxConnectionsPerHost',
    valid: [
      1, 10, 100, 1000, 10000, 10000n,
    ],
    invalid: [-1, -1n, 'a', null, false, true, {}, [], () => {}]
  },
  {
    key: 'maxConnectionsTotal',
    valid: [
      1, 10, 100, 1000, 10000, 10000n,
    ],
    invalid: [-1, -1n, 'a', null, false, true, {}, [], () => {}]
  },
  {
    key: 'maxStatelessResetsPerHost',
    valid: [
      1, 10, 100, 1000, 10000, 10000n,
    ],
    invalid: [-1, -1n, 'a', null, false, true, {}, [], () => {}]
  },
  {
    key: 'addressLRUSize',
    valid: [
      1, 10, 100, 1000, 10000, 10000n,
    ],
    invalid: [-1, -1n, 'a', null, false, true, {}, [], () => {}]
  },
  {
    key: 'maxRetries',
    valid: [
      1, 10, 100, 1000, 10000, 10000n,
    ],
    invalid: [-1, -1n, 'a', null, false, true, {}, [], () => {}]
  },
  {
    key: 'validateAddress',
    valid: [true, false, 0, 1, 'a'],
    invalid: [],
  },
  {
    key: 'disableStatelessReset',
    valid: [true, false, 0, 1, 'a'],
    invalid: [],
  },
  {
    key: 'ipv6Only',
    valid: [true, false, 0, 1, 'a'],
    invalid: [],
  },
  {
    key: 'udpReceiveBufferSize',
    valid: [0, 1, 2, 3, 4, 1000],
    invalid: [-1, 'a', null, false, true, {}, [], () => {}],
  },
  {
    key: 'udpSendBufferSize',
    valid: [0, 1, 2, 3, 4, 1000],
    invalid: [-1, 'a', null, false, true, {}, [], () => {}],
  },
  {
    key: 'udpTTL',
    valid: [0, 1, 2, 3, 4, 255],
    invalid: [-1, 256, 'a', null, false, true, {}, [], () => {}],
  },
  {
    key: 'resetTokenSecret',
    valid: [
      new Uint8Array(16),
      new Uint16Array(8),
      new Uint32Array(4),
    ],
    invalid: [
      'a', null, false, true, {}, [], () => {},
      new Uint8Array(15),
      new Uint8Array(17),
      new ArrayBuffer(16),
    ],
  },
  {
    key: 'tokenSecret',
    valid: [
      new Uint8Array(16),
      new Uint16Array(8),
      new Uint32Array(4),
    ],
    invalid: [
      'a', null, false, true, {}, [], () => {},
      new Uint8Array(15),
      new Uint8Array(17),
      new ArrayBuffer(16),
    ],
  },
  {
    // Unknown options are ignored entirely for any value type
    key: 'ignored',
    valid: ['a', null, false, true, {}, [], () => {}],
    invalid: [],
  },
];

for (const { key, valid, invalid } of cases) {
  for (const value of valid) {
    const options = {};
    options[key] = value;
    new QuicEndpoint(options);
  }

  for (const value of invalid) {
    const options = {};
    options[key] = value;
    throws(() => new QuicEndpoint(options), {
      message: new RegExp(`${key}`),
    }, value);
  }
}

// It can be inspected
const endpoint = new QuicEndpoint({});
strictEqual(typeof inspect(endpoint), 'string');
endpoint.close();
await endpoint.closed;

// Various address forms
new QuicEndpoint({
  address: { host: '127.0.0.1:0' },
});
new QuicEndpoint({
  address: '127.0.0.1:0',
});
throws(() => new QuicEndpoint({ address: 123 }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
