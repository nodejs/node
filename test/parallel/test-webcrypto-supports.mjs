import * as common from '../common/index.mjs';

if (!common.hasCrypto)
  common.skip('missing crypto');

import * as assert from 'node:assert';
const { SubtleCrypto } = globalThis;

const sources = [
  import('../fixtures/webcrypto/supports-level-2.mjs'),
  import('../fixtures/webcrypto/supports-secure-curves.mjs'),
  import('../fixtures/webcrypto/supports-modern-algorithms.mjs'),
  import('../fixtures/webcrypto/supports-sha3.mjs'),
];

const vectors = {};

for await (const mod of sources) {
  for (const op of Object.keys(mod.vectors)) {
    vectors[op] ||= [];
    vectors[op] = vectors[op].concat(mod.vectors[op]);
  }
}

vectors.verify = vectors.sign;
vectors.decrypt = vectors.encrypt;

for (const enc of vectors.encrypt) {
  for (const exp of vectors.exportKey) {
    vectors.wrapKey.push([enc[0] && exp[0], enc[1], exp[1]]);
  }
}

for (const dec of vectors.decrypt) {
  for (const imp of vectors.importKey) {
    vectors.unwrapKey.push([dec[0] && imp[0], dec[1], imp[1]]);
  }
}

for (const exportKey of vectors.exportKey) {
  if (!exportKey[0]) vectors.getPublicKey.push(exportKey);
}

for (const operation of Object.keys(vectors)) {
  for (const [expectation, ...args] of vectors[operation]) {
    assert.strictEqual(
      SubtleCrypto.supports(operation, ...args),
      expectation,
      new Error(
        `expected ${expectation}, got ${!expectation}`,
        { cause: { operation, args } }
      )
    );
  }
}
