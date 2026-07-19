'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  op: [
    'normalizeAlgorithm-string',
    'normalizeAlgorithm-dict',
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
  const { normalizeAlgorithm } = require('internal/crypto/util');

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
      // Exercises ensureSHA on a hash member.
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
