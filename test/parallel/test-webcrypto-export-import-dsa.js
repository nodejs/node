'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (common.hasOpenSSL3)
  common.skip('temporarily skipping for OpenSSL 3.0-alpha15');

const assert = require('assert');
const { subtle } = require('crypto').webcrypto;

const sizes = [1024];

const hashes = [
  'SHA-1',
  'SHA-256',
  'SHA-384',
  'SHA-512',
];

const keyData = {
  1024: {
    spki: Buffer.from(
      '308201c03082013406072a8648ce3804013082012702818100d5f35aa5730e26166fd' +
      '3ea81f8f0eeb05bd1250e164b7c76b180b6dae95096d13dee6956e15a9aea7cf18a0d' +
      'f7c5dc326ccef1cbf97636d22f870b76f2607f9a867db2756aecf65505aa48fdea5f5' +
      'ee54f508a05d9dae76bf262b4ca3662cc176b7c628c7bee2076df07f9a64e0402630d' +
      'fee63eaf0ed64d48b469fe1c9ac4a1021d00b14213226cfcfb59e3a0379e559c74ff8' +
      'a7383eb4c41cecb6f3732b702818100a0865b7f8954e7ae587c8e6a89e391e82657c5' +
      '8f05ccd94de61748e89e217efab3d9b5fa842ebc62525966916ad2b7af422a9b24078' +
      '17a5b382b6581434fd1a169c75ad4d0e3862a3f484e9f9f2a816f943a8e6060f26fe2' +
      '7c533587b765e57948439084e76fd6a4fd004f5c78d972cf7f100ec9494a902645bac' +
      'a4b4c6f399303818500028181009a8df69f2fe321869e2094e387bc1dc2b5f3bff2a2' +
      'e23cfba51d3c119fba6b4c15a49485fa811b6955d91d28c9e2e0445a79ddc5426b2fe' +
      '44e00a6c9254c776f13fd10dbc934262077b1df72c16bc848817c61fb6a607abe60c7' +
      'd11528ab9bdf55de45495733a047bd75a48b8166f1aa3deab681a2574a4f35106f0d7' +
      '8b641d7', 'hex'),
    pkcs8: Buffer.from(
      '3082015b0201003082013406072a8648ce3804013082012702818100d5f35aa5730e2' +
      '6166fd3ea81f8f0eeb05bd1250e164b7c76b180b6dae95096d13dee6956e15a9aea7c' +
      'f18a0df7c5dc326ccef1cbf97636d22f870b76f2607f9a867db2756aecf65505aa48f' +
      'dea5f5ee54f508a05d9dae76bf262b4ca3662cc176b7c628c7bee2076df07f9a64e04' +
      '02630dfee63eaf0ed64d48b469fe1c9ac4a1021d00b14213226cfcfb59e3a0379e559' +
      'c74ff8a7383eb4c41cecb6f3732b702818100a0865b7f8954e7ae587c8e6a89e391e8' +
      '2657c58f05ccd94de61748e89e217efab3d9b5fa842ebc62525966916ad2b7af422a9' +
      'b2407817a5b382b6581434fd1a169c75ad4d0e3862a3f484e9f9f2a816f943a8e6060' +
      'f26fe27c533587b765e57948439084e76fd6a4fd004f5c78d972cf7f100ec9494a902' +
      '645baca4b4c6f3993041e021c600daa0a9c4cc674c98bb07956374c84ac1c33af8816' +
      '3ea7e2587876', 'hex'),
  },
};

async function testImportSpki({ name, publicUsages }, size, hash, extractable) {
  const key = await subtle.importKey(
    'spki',
    keyData[size].spki,
    { name, hash },
    extractable,
    publicUsages);

  assert.strictEqual(key.type, 'public');
  assert.strictEqual(key.extractable, extractable);
  assert.deepStrictEqual(key.usages, publicUsages);
  assert.strictEqual(key.algorithm.name, name);
  assert.strictEqual(key.algorithm.modulusLength, size);
  assert.strictEqual(key.algorithm.hash.name, hash);

  if (extractable) {
    const spki = await subtle.exportKey('spki', key);
    assert.strictEqual(
      Buffer.from(spki).toString('hex'),
      keyData[size].spki.toString('hex'));
  } else {
    await assert.rejects(
      subtle.exportKey('spki', key), {
        message: /key is not extractable/
      });
  }
}

async function testImportPkcs8(
  { name, privateUsages },
  size,
  hash,
  extractable) {
  const key = await subtle.importKey(
    'pkcs8',
    keyData[size].pkcs8,
    { name, hash },
    extractable,
    privateUsages);

  assert.strictEqual(key.type, 'private');
  assert.strictEqual(key.extractable, extractable);
  assert.deepStrictEqual(key.usages, privateUsages);
  assert.strictEqual(key.algorithm.name, name);
  assert.strictEqual(key.algorithm.modulusLength, size);
  assert.strictEqual(key.algorithm.hash.name, hash);

  if (extractable) {
    const pkcs8 = await subtle.exportKey('pkcs8', key);
    assert.strictEqual(
      Buffer.from(pkcs8).toString('hex'),
      keyData[size].pkcs8.toString('hex'));
  } else {
    await assert.rejects(
      subtle.exportKey('pkcs8', key), {
        message: /key is not extractable/
      });
  }
}

// combinations to test
const testVectors = [
  {
    name: 'NODE-DSA',
    privateUsages: ['sign'],
    publicUsages: ['verify']
  },
];

(async function() {
  const variations = [];
  sizes.forEach((size) => {
    hashes.forEach((hash) => {
      [true, false].forEach((extractable) => {
        testVectors.forEach((vector) => {
          variations.push(testImportSpki(vector, size, hash, extractable));
          variations.push(testImportPkcs8(vector, size, hash, extractable));
        });
      });
    });
  });
  await Promise.all(variations);
})().then(common.mustCall());
