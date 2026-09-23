'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  op: [
    'normalizeAlgorithm-string',
    'normalizeAlgorithm-dict',
    'normalizeAlgorithm-validate-aes-gcm',
    'normalizeAlgorithm-validate-aes-cbc',
    'normalizeAlgorithm-validate-aes-ctr',
    'normalizeAlgorithm-validate-aes-generate',
    'normalizeAlgorithm-validate-hkdf',
    'normalizeAlgorithm-validate-hmac',
    'normalizeAlgorithm-validate-rsa',
    'webidl-dict',
    'webidl-algorithm-identifier-string',
    'webidl-algorithm-identifier-object',
    'webidl-dict-enforce-range',
    'webidl-dict-ensure-sha',
    'webidl-dict-null',
  ],
  n: [1e6],
}, { flags: ['--expose-internals'] });

function main({ n, op }) {
  const { normalizeAlgorithm, validateAlgorithm } = require('internal/crypto/util');

  switch (op) {
    case 'normalizeAlgorithm-string': {
      // String shortcut + null dictionary (cheapest path).
      bench.start();
      for (let i = 0; i < n; i++)
        normalizeAlgorithm('SHA-256', 'digest');
      bench.end(n);
      break;
    }
    case 'normalizeAlgorithm-dict': {
      // Object input with a dictionary type and no BufferSource members.
      const alg = { name: 'ECDSA', hash: 'SHA-256' };
      bench.start();
      for (let i = 0; i < n; i++)
        normalizeAlgorithm(alg, 'sign');
      bench.end(n);
      break;
    }
    case 'normalizeAlgorithm-validate-aes-gcm':
    case 'normalizeAlgorithm-validate-aes-cbc':
    case 'normalizeAlgorithm-validate-aes-ctr':
    case 'normalizeAlgorithm-validate-aes-generate':
    case 'normalizeAlgorithm-validate-hkdf':
    case 'normalizeAlgorithm-validate-hmac':
    case 'normalizeAlgorithm-validate-rsa': {
      const cases = {
        'aes-gcm': [
          { name: 'AES-GCM', iv: new Uint8Array(12), tagLength: 128 },
          'encrypt',
        ],
        'aes-cbc': [
          { name: 'AES-CBC', iv: new Uint8Array(16) },
          'encrypt',
        ],
        'aes-ctr': [
          { name: 'AES-CTR', counter: new Uint8Array(16), length: 64 },
          'encrypt',
        ],
        'aes-generate': [{ name: 'AES-GCM', length: 256 }, 'generateKey'],
        'hkdf': [
          {
            name: 'HKDF', hash: 'SHA-256',
            salt: new Uint8Array(32), info: new Uint8Array(32),
          },
          'deriveBits',
        ],
        'hmac': [{ name: 'HMAC', hash: 'SHA-256', length: 256 }, 'importKey'],
        'rsa': [
          {
            name: 'RSA-PSS', hash: 'SHA-256', modulusLength: 2048,
            publicExponent: new Uint8Array([1, 0, 1]),
          },
          'generateKey',
        ],
      };
      const name = op.slice('normalizeAlgorithm-validate-'.length);
      const [input, operation] = cases[name];
      bench.start();
      for (let i = 0; i < n; i++) {
        const normalized = normalizeAlgorithm(input, operation);
        // Older revisions validate inside normalizeAlgorithm.
        validateAlgorithm?.(normalized, operation);
      }
      bench.end(n);
      break;
    }
    case 'webidl-dict': {
      // WebIDL dictionary converter in isolation.
      const webidl = require('internal/crypto/webidl');
      const input = { name: 'AES-GCM', iv: new Uint8Array(12) };
      const opts = { prefix: 'test', context: 'test' };
      bench.start();
      for (let i = 0; i < n; i++)
        webidl.converters.AeadParams(input, opts);
      bench.end(n);
      break;
    }
    case 'webidl-algorithm-identifier-string': {
      // Exercises converters.AlgorithmIdentifier string path.
      const webidl = require('internal/crypto/webidl');
      const opts = { prefix: 'test', context: 'test' };
      bench.start();
      for (let i = 0; i < n; i++)
        webidl.converters.AlgorithmIdentifier('SHA-256', opts);
      bench.end(n);
      break;
    }
    case 'webidl-algorithm-identifier-object': {
      // Exercises converters.AlgorithmIdentifier object path.
      const webidl = require('internal/crypto/webidl');
      const input = { name: 'SHA-256' };
      const opts = { prefix: 'test', context: 'test' };
      bench.start();
      for (let i = 0; i < n; i++)
        webidl.converters.AlgorithmIdentifier(input, opts);
      bench.end(n);
      break;
    }
    case 'webidl-dict-enforce-range': {
      // Exercises [EnforceRange] integer dictionary members.
      const webidl = require('internal/crypto/webidl');
      const input = {
        name: 'RSASSA-PKCS1-v1_5',
        modulusLength: 2048,
        publicExponent: new Uint8Array([1, 0, 1]),
      };
      const opts = { prefix: 'test', context: 'test' };
      bench.start();
      for (let i = 0; i < n; i++)
        webidl.converters.RsaKeyGenParams(input, opts);
      bench.end(n);
      break;
    }
    case 'webidl-dict-ensure-sha': {
      // Converts a dictionary containing a hash identifier.
      const webidl = require('internal/crypto/webidl');
      const input = { name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' };
      const opts = { prefix: 'test', context: 'test' };
      bench.start();
      for (let i = 0; i < n; i++)
        webidl.converters.RsaHashedImportParams(input, opts);
      bench.end(n);
      break;
    }
    case 'webidl-dict-null': {
      // Exercises the null/undefined path in createDictionaryConverter().
      const webidl = require('internal/crypto/webidl');
      const opts = { prefix: 'test', context: 'test' };
      bench.start();
      for (let i = 0; i < n; i++)
        webidl.converters.JsonWebKey(undefined, opts);
      bench.end(n);
      break;
    }
  }
}
