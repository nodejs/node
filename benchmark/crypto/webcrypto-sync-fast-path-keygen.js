'use strict';

const common = require('../common.js');
const {
  isSupported,
  measureAsync,
} = require('./_webcrypto_sync_fast_path_common.js');

const { subtle } = globalThis.crypto;

const keygenCases = {
  'aes-cbc-128': {
    algorithm: { name: 'AES-CBC', length: 128 },
    usages: ['encrypt', 'decrypt'],
  },
  'aes-ctr-128': {
    algorithm: { name: 'AES-CTR', length: 128 },
    usages: ['encrypt', 'decrypt'],
  },
  'aes-gcm-128': {
    algorithm: { name: 'AES-GCM', length: 128 },
    usages: ['encrypt', 'decrypt'],
  },
  'aes-kw-128': {
    algorithm: { name: 'AES-KW', length: 128 },
    usages: ['wrapKey', 'unwrapKey'],
  },
  'aes-ocb-128': {
    algorithm: { name: 'AES-OCB', length: 128 },
    usages: ['encrypt', 'decrypt'],
  },
  'chacha20-poly1305': {
    algorithm: { name: 'ChaCha20-Poly1305' },
    usages: ['encrypt', 'decrypt'],
  },
  'hmac': {
    algorithm: { name: 'HMAC', hash: 'SHA-256', length: 256 },
    usages: ['sign', 'verify'],
  },
  'kmac128': {
    algorithm: { name: 'KMAC128', length: 128 },
    usages: ['sign', 'verify'],
  },
  'kmac256': {
    algorithm: { name: 'KMAC256', length: 256 },
    usages: ['sign', 'verify'],
  },
  'ecdsa-p256': {
    algorithm: { name: 'ECDSA', namedCurve: 'P-256' },
    usages: ['sign', 'verify'],
  },
  'ecdsa-p384': {
    algorithm: { name: 'ECDSA', namedCurve: 'P-384' },
    usages: ['sign', 'verify'],
  },
  'ecdsa-p521': {
    algorithm: { name: 'ECDSA', namedCurve: 'P-521' },
    usages: ['sign', 'verify'],
  },
  'ecdh-p256': {
    algorithm: { name: 'ECDH', namedCurve: 'P-256' },
    usages: ['deriveBits'],
  },
  'ecdh-p384': {
    algorithm: { name: 'ECDH', namedCurve: 'P-384' },
    usages: ['deriveBits'],
  },
  'ecdh-p521': {
    algorithm: { name: 'ECDH', namedCurve: 'P-521' },
    usages: ['deriveBits'],
  },
  'ed25519': {
    algorithm: { name: 'Ed25519' },
    usages: ['sign', 'verify'],
  },
  'ed448': {
    algorithm: { name: 'Ed448' },
    usages: ['sign', 'verify'],
  },
  'x25519': {
    algorithm: { name: 'X25519' },
    usages: ['deriveBits'],
  },
  'x448': {
    algorithm: { name: 'X448' },
    usages: ['deriveBits'],
  },
  'ml-dsa-44': {
    algorithm: { name: 'ML-DSA-44' },
    usages: ['sign', 'verify'],
  },
  'ml-dsa-65': {
    algorithm: { name: 'ML-DSA-65' },
    usages: ['sign', 'verify'],
  },
  'ml-dsa-87': {
    algorithm: { name: 'ML-DSA-87' },
    usages: ['sign', 'verify'],
  },
  'ml-kem-512': {
    algorithm: { name: 'ML-KEM-512' },
    usages: ['encapsulateBits', 'decapsulateBits'],
  },
  'ml-kem-768': {
    algorithm: { name: 'ML-KEM-768' },
    usages: ['encapsulateBits', 'decapsulateBits'],
  },
  'ml-kem-1024': {
    algorithm: { name: 'ML-KEM-1024' },
    usages: ['encapsulateBits', 'decapsulateBits'],
  },
};

const algorithms = Object.keys(keygenCases)
  .filter((name) => isSupported('generateKey', keygenCases[name].algorithm));

if (algorithms.length === 0) {
  console.log('no supported WebCrypto keygen algorithms available');
  process.exit(0);
}

const bench = common.createBenchmark(main, {
  algorithm: algorithms,
  mode: ['serial', 'parallel'],
  n: [1e3],
});

async function main({ n, algorithm, mode }) {
  const { algorithm: keyAlgorithm, usages } = keygenCases[algorithm];
  await measureAsync(
    bench,
    n,
    mode,
    () => subtle.generateKey(keyAlgorithm, false, usages));
}
