'use strict';

const common = require('../common.js');
const assert = require('node:assert');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

const fixtureDir = path.resolve(__dirname, '../../test/fixtures/keys');
const certificate = fs.readFileSync(path.join(fixtureDir, 'agent1-cert.pem'));
const dhPrime = crypto.getDiffieHellman('modp14').getPrime();
const key = Buffer.alloc(32, 0x01);
const keyObject = crypto.createSecretKey(key);
const hmacAlgorithm = { name: 'HMAC', hash: 'SHA-256' };
const keyUsages = ['sign'];
const iv = Buffer.alloc(16, 0x02);

const iterations = {
  Certificate: 1e7,
  Cipheriv: 1e5,
  Decipheriv: 1e5,
  DiffieHellman: 10,
  DiffieHellmanGroup: 2e5,
  ECDH: 2e5,
  Hash: 1e5,
  Hmac: 5e4,
  KeyObject: 1e5,
  Sign: 1e5,
  Verify: 1e5,
  CryptoKey: 5e4,
  X509Certificate: 5e3,
};

const bench = common.createBenchmark(main, {
  type: Object.keys(iterations),
  n: [...new Set(Object.values(iterations))],
}, {
  combinationFilter({ type, n }) {
    // Benchmark test mode reduces numeric options to 1.
    return n === 1 || iterations[type] === n;
  },
});

function construct(type) {
  switch (type) {
    case 'Certificate':
      return new crypto.Certificate();
    case 'Cipheriv':
      return crypto.createCipheriv('aes-256-ctr', key, iv);
    case 'Decipheriv':
      return crypto.createDecipheriv('aes-256-ctr', key, iv);
    case 'DiffieHellman':
      return crypto.createDiffieHellman(dhPrime);
    case 'DiffieHellmanGroup':
      return crypto.getDiffieHellman('modp14');
    case 'ECDH':
      return crypto.createECDH('prime256v1');
    case 'Hash':
      return crypto.createHash('sha256');
    case 'Hmac':
      return crypto.createHmac('sha256', key);
    case 'KeyObject':
      return crypto.createSecretKey(key);
    case 'Sign':
      return crypto.createSign('sha256');
    case 'Verify':
      return crypto.createVerify('sha256');
    case 'CryptoKey':
      return keyObject.toCryptoKey(hmacAlgorithm, true, keyUsages);
    case 'X509Certificate':
      return new crypto.X509Certificate(certificate);
    default:
      throw new Error(`Unsupported class: ${type}`);
  }
}

function main({ type, n }) {
  const instances = new Array(n);

  bench.start();
  for (let i = 0; i < n; ++i)
    instances[i] = construct(type);
  bench.end(n);

  assert.strictEqual(typeof instances[n - 1], 'object');
}
