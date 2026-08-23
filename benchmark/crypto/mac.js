'use strict';

const common = require('../common.js');
const { hasOpenSSL } = require('../../test/common/crypto.js');
const assert = require('node:assert');
const {
  createHmac,
  createMac,
  getMacs,
} = require('node:crypto');

if (!hasOpenSSL(3) ||
    process.features.openssl_is_boringssl ||
    typeof createMac !== 'function' ||
    typeof getMacs !== 'function') {
  console.log('Skipping: generic MAC API requires OpenSSL >= 3');
  process.exit(0);
}

const operations = [
  'get-macs-cold',
  'get-macs-warm',
  'create-cold',
  'create-warm',
  'hmac-lifecycle',
  'mac-lifecycle',
  'mac-stream-lifecycle',
  'update',
  'stream',
  'final-buffer',
  'final-hex',
];
const configurations = {
  'hmac-sha256': {
    algorithm: 'HMAC',
    key: Buffer.alloc(32, 0x42),
    options: { digest: 'SHA256' },
  },
  'kmac-128': {
    algorithm: 'KMAC-128',
    key: Buffer.alloc(32, 0x42),
    options: { outputLength: 32 },
  },
};

const bench = common.createBenchmark(main, {
  operation: operations,
  algorithm: Object.keys(configurations),
  length: [0, 64, 4096],
  n: [1, 10_000, 20_000, 500_000],
}, {
  combinationFilter({ operation, algorithm, length, n }) {
    if (operation === 'get-macs-cold') {
      return algorithm === 'hmac-sha256' && length === 0 && n === 1;
    }
    if (operation === 'get-macs-warm') {
      return algorithm === 'hmac-sha256' && length === 0 && n === 500_000;
    }
    if (operation === 'create-cold')
      return length === 0 && n === 1;
    if (operation === 'create-warm')
      return length === 0 && n === 20_000;
    if (operation === 'hmac-lifecycle') {
      return algorithm === 'hmac-sha256' && n === 10_000;
    }
    if (operation === 'mac-lifecycle' ||
        operation === 'mac-stream-lifecycle') {
      return n === 10_000;
    }
    if (operation === 'update' || operation === 'stream') {
      return length === 64 && n === 500_000;
    }
    if (operation === 'final-buffer' || operation === 'final-hex') {
      return algorithm === 'hmac-sha256' &&
             length === 64 &&
             n === 20_000;
    }
    return false;
  },
  test: {
    operation: ['create-cold'],
    algorithm: ['hmac-sha256'],
    length: [0],
    n: [1],
  },
});

function main({ operation, algorithm, length, n }) {
  const configuration = configurations[algorithm];
  const data = Buffer.alloc(length, 0x61);

  switch (operation) {
    case 'get-macs-cold':
      measureGetMacs(n, false);
      break;
    case 'get-macs-warm':
      measureGetMacs(n, true);
      break;
    case 'create-cold':
      measureCreate(configuration, n, false);
      break;
    case 'create-warm':
      measureCreate(configuration, n, true);
      break;
    case 'hmac-lifecycle':
      measureHmacLifecycle(configuration, data, n);
      break;
    case 'mac-lifecycle':
      measureMacLifecycle(configuration, data, n);
      break;
    case 'mac-stream-lifecycle':
      measureMacStreamLifecycle(configuration, data, n);
      break;
    case 'update':
      measureUpdate(configuration, data, n);
      break;
    case 'stream':
      measureStream(configuration, data, n);
      break;
    case 'final-buffer':
      measureFinal(configuration, data, n);
      break;
    case 'final-hex':
      measureFinal(configuration, data, n, 'hex');
      break;
    default:
      throw new Error(`unknown operation: ${operation}`);
  }
}

function measureGetMacs(n, warm) {
  if (warm)
    getMacs();

  let result;
  bench.start();
  for (let i = 0; i < n; ++i)
    result = getMacs();
  bench.end(n);

  assert(Array.isArray(result));
}

function measureCreate({ algorithm, key, options }, n, warm) {
  if (warm)
    createMac(algorithm, key, options).final();

  const contexts = new Array(n);
  bench.start();
  for (let i = 0; i < n; ++i)
    contexts[i] = createMac(algorithm, key, options);
  bench.end(n);

  assert.strictEqual(typeof contexts[n - 1], 'object');
}

function measureHmacLifecycle({ key, options }, data, n) {
  createHmac(options.digest, key).update(data).digest();

  let result;
  bench.start();
  for (let i = 0; i < n; ++i)
    result = createHmac(options.digest, key).update(data).digest();
  bench.end(n);

  assert(Buffer.isBuffer(result));
}

function measureMacLifecycle({ algorithm, key, options }, data, n) {
  createMac(algorithm, key, options).update(data).final();

  let result;
  bench.start();
  for (let i = 0; i < n; ++i)
    result = createMac(algorithm, key, options).update(data).final();
  bench.end(n);

  assert(Buffer.isBuffer(result));
}

function measureMacStreamLifecycle({ algorithm, key, options }, data, n) {
  const warmup = createMac(algorithm, key, options);
  warmup.end(data);
  warmup.read();

  let result;
  bench.start();
  for (let i = 0; i < n; ++i) {
    const context = createMac(algorithm, key, options);
    context.end(data);
    result = context.read();
  }
  bench.end(n);

  assert(Buffer.isBuffer(result));
}

function measureUpdate({ algorithm, key, options }, data, n) {
  const warmup = createMac(algorithm, key, options);
  warmup.update(data).final();

  const context = createMac(algorithm, key, options);
  bench.start();
  for (let i = 0; i < n; ++i)
    context.update(data);
  bench.end(n);

  assert(Buffer.isBuffer(context.final()));
}

function measureStream({ algorithm, key, options }, data, n) {
  const warmup = createMac(algorithm, key, options);
  warmup.end(data);
  warmup.read();

  const context = createMac(algorithm, key, options);
  bench.start();
  for (let i = 0; i < n; ++i)
    context.write(data);
  bench.end(n);

  context.end();
  assert(Buffer.isBuffer(context.read()));
}

function measureFinal({ algorithm, key, options }, data, n, encoding) {
  const warmup = createMac(algorithm, key, options).update(data);
  if (encoding === undefined)
    warmup.final();
  else
    warmup.final(encoding);

  const contexts = new Array(n);
  for (let i = 0; i < n; ++i)
    contexts[i] = createMac(algorithm, key, options).update(data);

  let result;
  if (encoding === undefined) {
    bench.start();
    for (let i = 0; i < n; ++i)
      result = contexts[i].final();
    bench.end(n);
  } else {
    bench.start();
    for (let i = 0; i < n; ++i)
      result = contexts[i].final(encoding);
    bench.end(n);
  }

  if (encoding === undefined)
    assert(Buffer.isBuffer(result));
  else
    assert.strictEqual(typeof result, 'string');
}
