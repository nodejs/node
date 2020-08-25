'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = require('crypto').webcrypto;

const sizes = [1024];

const hashes = [
  'SHA-1',
  'SHA-256',
  'SHA-384',
  'SHA-512'
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
    jwk: {
      kty: 'DSA',
      y: 'mo32ny_jIYaeIJTjh7wdwrXzv_Ki4jz7pR08EZ-6a0wVpJSF-oEbaVXZHSjJ4uBEWn' +
         'ndxUJrL-ROAKbJJUx3bxP9ENvJNCYgd7HfcsFryEiBfGH7amB6vmDH0RUoq5vfVd5F' +
         'SVczoEe9daSLgWbxqj3qtoGiV0pPNRBvDXi2Qdc',
      p: '1fNapXMOJhZv0-qB-PDusFvRJQ4WS3x2sYC22ulQltE97mlW4Vqa6nzxig33xdwybM' +
         '7xy_l2NtIvhwt28mB_moZ9snVq7PZVBapI_epfXuVPUIoF2drna_JitMo2YswXa3xi' +
         'jHvuIHbfB_mmTgQCYw3-5j6vDtZNSLRp_hyaxKE',
      q: 'sUITImz8-1njoDeeVZx0_4pzg-tMQc7Lbzcytw',
      g: 'oIZbf4lU565YfI5qieOR6CZXxY8FzNlN5hdI6J4hfvqz2bX6hC68YlJZZpFq0revQi' +
         'qbJAeBels4K2WBQ0_RoWnHWtTQ44YqP0hOn58qgW-UOo5gYPJv4nxTNYe3ZeV5SEOQ' +
         'hOdv1qT9AE9ceNlyz38QDslJSpAmRbrKS0xvOZM',
      x: 'YA2qCpxMxnTJi7B5VjdMhKwcM6-IFj6n4lh4dg',
    }
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

async function testImportJwk(
  { name, publicUsages, privateUsages },
  size,
  hash,
  extractable) {

  const jwk = keyData[size].jwk;

  const [
    publicKey,
    privateKey,
  ] = await Promise.all([
    subtle.importKey(
      'jwk',
      {
        kty: jwk.kty,
        y: jwk.y,
        p: jwk.p,
        q: jwk.q,
        g: jwk.g,
        alg: `NODE-DSA-${hash}`
      },
      { name, hash },
      extractable,
      publicUsages),
    subtle.importKey(
      'jwk',
      { ...jwk, alg: `NODE-DSA-${hash}` },
      { name, hash },
      extractable,
      privateUsages)
  ]);

  assert.strictEqual(publicKey.type, 'public');
  assert.strictEqual(privateKey.type, 'private');
  assert.strictEqual(publicKey.extractable, extractable);
  assert.strictEqual(privateKey.extractable, extractable);
  assert.strictEqual(publicKey.algorithm.name, name);
  assert.strictEqual(privateKey.algorithm.name, name);
  assert.strictEqual(publicKey.algorithm.modulusLength, size);
  assert.strictEqual(privateKey.algorithm.modulusLength, size);

  if (extractable) {
    const [
      pubJwk,
      pvtJwk
    ] = await Promise.all([
      subtle.exportKey('jwk', publicKey),
      subtle.exportKey('jwk', privateKey)
    ]);

    assert.strictEqual(pubJwk.kty, 'DSA');
    assert.strictEqual(pvtJwk.kty, 'DSA');
    assert.strictEqual(pubJwk.y, jwk.y);
    assert.strictEqual(pvtJwk.y, jwk.y);
    assert.strictEqual(pubJwk.p, jwk.p);
    assert.strictEqual(pvtJwk.p, jwk.p);
    assert.strictEqual(pubJwk.q, jwk.q);
    assert.strictEqual(pvtJwk.q, jwk.q);
    assert.strictEqual(pubJwk.g, jwk.g);
    assert.strictEqual(pvtJwk.g, jwk.g);
    assert.strictEqual(pvtJwk.x, jwk.x);
    assert.strictEqual(pubJwk.x, undefined);
  } else {
    await assert.rejects(
      subtle.exportKey('jwk', publicKey), {
        message: /key is not extractable/
      });
    await assert.rejects(
      subtle.exportKey('jwk', privateKey), {
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
  }
];

(async function() {
  const variations = [];
  sizes.forEach((size) => {
    hashes.forEach((hash) => {
      [true, false].forEach((extractable) => {
        testVectors.forEach((vector) => {
          variations.push(testImportSpki(vector, size, hash, extractable));
          variations.push(testImportPkcs8(vector, size, hash, extractable));
          variations.push(testImportJwk(vector, size, hash, extractable));
        });
      });
    });
  });
  await Promise.all(variations);
})().then(common.mustCall());
