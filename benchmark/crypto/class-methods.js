'use strict';

const common = require('../common.js');
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');

const fixtureDir = path.resolve(__dirname, '../../test/fixtures/keys');
const certificate = fs.readFileSync(path.join(fixtureDir, 'agent1-cert.pem'));
const key = Buffer.alloc(32, 0x01);
const hmacAlgorithm = { name: 'HMAC', hash: 'SHA-256' };
const keyUsages = ['sign'];
const iv = Buffer.alloc(16, 0x02);
const input = Buffer.alloc(16, 0x03);

const iterations = {
  'Cipheriv-update': 1e6,
  'Decipheriv-update': 1e6,
  'DiffieHellman-getGenerator': 2e5,
  'DiffieHellmanGroup-getGenerator': 2e6,
  'ECDH-getPrivateKey': 2e6,
  'Hash-update': 5e6,
  'Hmac-update': 5e6,
  'KeyObject-equals': 2e6,
  'KeyObject-symmetricKeySize-first': 1e5,
  'KeyObject-type-first': 1e5,
  'KeyObject-type': 1e8,
  'Sign-update': 5e6,
  'Verify-update': 5e6,
  'CryptoKey-toKeyObject': 2e5,
  'CryptoKey-algorithm-first': 1e5,
  'CryptoKey-extractable-first': 1e5,
  'CryptoKey-type-first': 1e5,
  'CryptoKey-usages-first': 1e5,
  'CryptoKey-type': 1e8,
  'X509Certificate-checkHost': 1e6,
  'X509Certificate-publicKey-first': 5e3,
  'X509Certificate-publicKey': 1e8,
  'X509Certificate-subject-first': 5e3,
  'X509Certificate-subject': 1e8,
};

const bench = common.createBenchmark(main, {
  operation: Object.keys(iterations),
  n: [...new Set(Object.values(iterations))],
}, {
  combinationFilter({ operation, n }) {
    // Benchmark test mode reduces numeric options to 1.
    return n === 1 || iterations[operation] === n;
  },
});

function setup(operation) {
  switch (operation) {
    case 'Cipheriv-update': {
      const cipher = crypto.createCipheriv('aes-256-ctr', key, iv);
      return {
        run: () => cipher.update(input),
        finish: () => cipher.final(),
      };
    }
    case 'Decipheriv-update': {
      const decipher = crypto.createDecipheriv('aes-256-ctr', key, iv);
      return {
        run: () => decipher.update(input),
        finish: () => decipher.final(),
      };
    }
    case 'DiffieHellman-getGenerator': {
      const dh = crypto.createDiffieHellman(
        crypto.getDiffieHellman('modp14').getPrime());
      return { run: () => dh.getGenerator() };
    }
    case 'DiffieHellmanGroup-getGenerator': {
      const dh = crypto.getDiffieHellman('modp14');
      return { run: () => dh.getGenerator() };
    }
    case 'ECDH-getPrivateKey': {
      const ecdh = crypto.createECDH('prime256v1');
      ecdh.generateKeys();
      return { run: () => ecdh.getPrivateKey() };
    }
    case 'Hash-update': {
      const hash = crypto.createHash('sha256');
      return {
        run: () => hash.update(input),
        finish: () => hash.digest(),
      };
    }
    case 'Hmac-update': {
      const hmac = crypto.createHmac('sha256', key);
      return {
        run: () => hmac.update(input),
        finish: () => hmac.digest(),
      };
    }
    case 'KeyObject-equals': {
      const keyObject = crypto.createSecretKey(key);
      return { run: () => keyObject.equals(keyObject) };
    }
    case 'KeyObject-symmetricKeySize-first': {
      return { run: () => crypto.createSecretKey(key).symmetricKeySize };
    }
    case 'KeyObject-type-first': {
      return { run: () => crypto.createSecretKey(key).type };
    }
    case 'KeyObject-type': {
      const keyObjects = Array.from(
        { length: 64 }, () => crypto.createSecretKey(key));
      for (const keyObject of keyObjects) {
        if (keyObject.type !== 'secret')
          throw new Error('Unexpected KeyObject type');
      }
      let index = 0;
      return { run: () => keyObjects[index++ & 63].type };
    }
    case 'Sign-update': {
      const sign = crypto.createSign('sha256');
      return { run: () => sign.update(input) };
    }
    case 'Verify-update': {
      const verify = crypto.createVerify('sha256');
      return { run: () => verify.update(input) };
    }
    case 'CryptoKey-toKeyObject': {
      const keyObject = crypto.createSecretKey(key);
      const cryptoKey = keyObject.toCryptoKey(
        hmacAlgorithm, true, keyUsages);
      return { run: () => crypto.KeyObject.from(cryptoKey) };
    }
    case 'CryptoKey-algorithm-first':
    case 'CryptoKey-extractable-first':
    case 'CryptoKey-type-first':
    case 'CryptoKey-usages-first': {
      const keyObject = crypto.createSecretKey(key);
      const property = operation.slice('CryptoKey-'.length, -'-first'.length);
      return {
        run: () => keyObject.toCryptoKey(
          hmacAlgorithm, true, keyUsages)[property],
      };
    }
    case 'CryptoKey-type': {
      const keyObject = crypto.createSecretKey(key);
      const cryptoKeys = Array.from(
        { length: 64 },
        () => keyObject.toCryptoKey(hmacAlgorithm, true, keyUsages));
      for (const cryptoKey of cryptoKeys) {
        if (cryptoKey.type !== 'secret')
          throw new Error('Unexpected CryptoKey type');
      }
      let index = 0;
      return { run: () => cryptoKeys[index++ & 63].type };
    }
    case 'X509Certificate-checkHost': {
      const x509 = new crypto.X509Certificate(certificate);
      return { run: () => x509.checkHost('agent1') };
    }
    case 'X509Certificate-publicKey-first': {
      return {
        run: () => new crypto.X509Certificate(certificate).publicKey,
      };
    }
    case 'X509Certificate-publicKey': {
      const certificates = Array.from(
        { length: 64 }, () => new crypto.X509Certificate(certificate));
      for (const x509 of certificates) {
        if (x509.publicKey === undefined)
          throw new Error('Missing certificate public key');
      }
      let index = 0;
      return { run: () => certificates[index++ & 63].publicKey };
    }
    case 'X509Certificate-subject-first': {
      return {
        run: () => new crypto.X509Certificate(certificate).subject,
      };
    }
    case 'X509Certificate-subject': {
      const certificates = Array.from(
        { length: 64 }, () => new crypto.X509Certificate(certificate));
      for (const x509 of certificates) {
        if (x509.subject === undefined)
          throw new Error('Missing certificate subject');
      }
      let index = 0;
      return { run: () => certificates[index++ & 63].subject };
    }
    default:
      throw new Error(`Unsupported operation: ${operation}`);
  }
}

function main({ operation, n }) {
  const state = setup(operation);
  let result;

  bench.start();
  for (let i = 0; i < n; ++i)
    result = state.run();
  bench.end(n);

  if (state.finish)
    state.finish();
  if (result === state)
    throw new Error('Unexpected benchmark result');
}
