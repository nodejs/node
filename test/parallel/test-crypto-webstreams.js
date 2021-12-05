'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const {
  deepStrictEqual,
  strictEqual,
  rejects,
  throws,
} = require('assert');

const {
  webcrypto: {
    subtle,
  },
} = require('crypto');

const {
  DigestStream,
  SignStream,
  VerifyStream,
} = require('crypto/stream');

{
  const digests = {
    'SHA-256': new Uint8Array([
      0x2c, 0xf2, 0x4d, 0xba, 0x5f, 0xb0, 0xa3, 0x0e,
      0x26, 0xe8, 0x3b, 0x2a, 0xc5, 0xb9, 0xe2, 0x9e,
      0x1b, 0x16, 0x1e, 0x5c, 0x1f, 0xa7, 0x42, 0x5e,
      0x73, 0x04, 0x33, 0x62, 0x93, 0x8b, 0x98, 0x24]),
    'SHA-384': new Uint8Array([
      0x59, 0xe1, 0x74, 0x87, 0x77, 0x44, 0x8c, 0x69,
      0xde, 0x6b, 0x80, 0x0d, 0x7a, 0x33, 0xbb, 0xfb,
      0x9f, 0xf1, 0xb4, 0x63, 0xe4, 0x43, 0x54, 0xc3,
      0x55, 0x3b, 0xcd, 0xb9, 0xc6, 0x66, 0xfa, 0x90,
      0x12, 0x5a, 0x3c, 0x79, 0xf9, 0x03, 0x97, 0xbd,
      0xf5, 0xf6, 0xa1, 0x3d, 0xe8, 0x28, 0x68, 0x4f]),
    'SHA-512': new Uint8Array([
      0x9b, 0x71, 0xd2, 0x24, 0xbd, 0x62, 0xf3, 0x78,
      0x5d, 0x96, 0xd4, 0x6a, 0xd3, 0xea, 0x3d, 0x73,
      0x31, 0x9b, 0xfb, 0xc2, 0x89, 0x0c, 0xaa, 0xda,
      0xe2, 0xdf, 0xf7, 0x25, 0x19, 0x67, 0x3c, 0xa7,
      0x23, 0x23, 0xc3, 0xd9, 0x9b, 0xa5, 0xc1, 0x1d,
      0x7c, 0x7a, 0xcc, 0x6e, 0x14, 0xb8, 0xc5, 0xda,
      0x0c, 0x46, 0x63, 0x47, 0x5c, 0x2e, 0x5c, 0x3a,
      0xde, 0xf4, 0x6f, 0x73, 0xbc, 0xde, 0xc0, 0x43]),
  };

  Object.keys(digests).forEach(async (i) => {
    const stream = new DigestStream(i);
    const writer = stream.getWriter();
    await writer.write('hello');
    await writer.close();
    const digest = await stream.digest;
    strictEqual(digest.byteLength, digests[i].byteLength);
    deepStrictEqual(new Uint8Array(digest), digests[i]);
  });

  const stream = new DigestStream('SHA-256');
  const writer = stream.getWriter();
  rejects(writer.write({ 'wrong': 'datatype' }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
}

{
  const publicExponent = new Uint8Array([1, 0, 1]);

  async function generateRsaKey(modulusLength = 2048, hash = 'SHA-256') {
    const {
      publicKey,
      privateKey
    } = await subtle.generateKey({
      name: 'RSASSA-PKCS1-v1_5',
      modulusLength,
      publicExponent,
      hash,
    }, true, ['sign', 'verify']);

    return { publicKey, privateKey };
  }

  (async () => {
    const {
      publicKey,
      privateKey,
    } = await generateRsaKey();

    const sign = new SignStream(
      {
        name: privateKey.algorithm.name,
      },
      privateKey);

    const writer1 = sign.getWriter();

    await writer1.write('hello');
    await writer1.close();

    const sig = await sign.signature;

    const verify = new VerifyStream(
      {
        name: publicKey.algorithm.name,
      },
      publicKey,
      sig);

    const writer2 = verify.getWriter();

    await writer2.write('hello');
    await writer2.close();

    strictEqual(await verify.verified, true);
  })().then();
}

{
  const publicExponent = new Uint8Array([1, 0, 1]);

  async function generateRsaKey(modulusLength = 2048, hash = 'SHA-256') {
    const {
      publicKey,
      privateKey
    } = await subtle.generateKey({
      name: 'RSA-PSS',
      modulusLength,
      publicExponent,
      hash,
    }, true, ['sign', 'verify']);

    return { publicKey, privateKey };
  }

  (async () => {
    const {
      publicKey,
      privateKey,
    } = await generateRsaKey();

    const sign = new SignStream(
      {
        name: privateKey.algorithm.name,
        saltLength: 10,
      },
      privateKey);

    const writer1 = sign.getWriter();

    await writer1.write('hello');
    await writer1.close();

    const sig = await sign.signature;

    const verify = new VerifyStream(
      {
        name: publicKey.algorithm.name,
        saltLength: 10,
      },
      publicKey,
      sig);

    const writer2 = verify.getWriter();

    await writer2.write('hello');
    await writer2.close();

    strictEqual(await verify.verified, true);
  })().then();
}

{
  async function generateEcKey(namedCurve = 'P-521') {
    const {
      publicKey,
      privateKey
    } = await subtle.generateKey({
      name: 'ECDSA',
      namedCurve,
    }, true, ['sign', 'verify']);

    return { publicKey, privateKey };
  }

  (async () => {
    const {
      publicKey,
      privateKey,
    } = await generateEcKey();

    const sign = new SignStream(
      {
        name: privateKey.algorithm.name,
        hash: 'SHA-256',
      },
      privateKey);

    const writer1 = sign.getWriter();

    await writer1.write('hello');
    await writer1.close();

    const sig = await sign.signature;

    const verify = new VerifyStream(
      {
        name: publicKey.algorithm.name,
        hash: 'SHA-256',
      },
      publicKey,
      sig);

    const writer2 = verify.getWriter();

    await writer2.write('hello');
    await writer2.close();

    strictEqual(await verify.verified, true);
  })().then();
}

{
  async function generateDsaKey() {
    const {
      publicKey,
      privateKey
    } = await subtle.generateKey({
      name: 'NODE-DSA',
      hash: 'SHA-256',
      modulusLength: 2048,
    }, true, ['sign', 'verify']);

    return { publicKey, privateKey };
  }

  (async () => {
    const {
      publicKey,
      privateKey,
    } = await generateDsaKey();

    const sign = new SignStream(
      {
        name: privateKey.algorithm.name,
      },
      privateKey);

    const writer1 = sign.getWriter();

    await writer1.write('hello');
    await writer1.close();

    const sig = await sign.signature;

    const verify = new VerifyStream(
      {
        name: publicKey.algorithm.name,
      },
      publicKey,
      sig);

    const writer2 = verify.getWriter();

    await writer2.write('hello');
    await writer2.close();

    strictEqual(await verify.verified, true);
  })().then();
}

throws(() => new DigestStream('not a digest'), {
  name: 'NotSupportedError',
  message: 'Unrecognized name.',
});

throws(() => new DigestStream({ name: 'not a digest' }), {
  name: 'NotSupportedError',
  message: 'Unrecognized name.',
});

throws(() => new SignStream({ name: 'not a digest' }), {
  name: 'NotSupportedError',
  message: 'Unrecognized name.',
});

throws(() => new VerifyStream({ name: 'not a digest' }), {
  name: 'NotSupportedError',
  message: 'Unrecognized name.',
});

throws(() => new SignStream('SHA-256', 'not a key'), {
  code: 'ERR_INVALID_ARG_TYPE',
});

throws(() => new VerifyStream('SHA-256', 'not a key'), {
  code: 'ERR_INVALID_ARG_TYPE',
});
