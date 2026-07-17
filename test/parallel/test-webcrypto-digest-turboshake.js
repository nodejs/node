'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

const kSourceData = {
  empty: '',
  short: '156eea7cc14c56cb94db030a4a9d95ff',
  medium: 'b6c8f9df648cd088b70f38e74197b18cb81e1e435' +
          '0d50bccb8fb5a7379c87bb2e3d6ed5461ed1e9f36' +
          'f340a3962a446b815b794b4bd43a4403502077b22' +
          '56cc807837f3aacd118eb4b9c2baeb897068625ab' +
          'aca193',
  long: null
};

kSourceData.long = kSourceData.medium.repeat(1024);

// Test vectors generated with PyCryptodome as a reference implementation
const kDigestedData = {
  'turboshake128': {
    empty: '1e415f1c5983aff2169217277d17bb538cd945a397ddec541f1ce41af2c1b74c',
    short: 'f8d1ebf3b48b71b0514686090eb25f1de322a00149be9b4dc5f09ac9077cd8a8',
    medium: '0d0e7eceb4ae58c3c48f6c2bad56d0f8ff3f887468d3ea55a138aedf395233c0',
    long: '5747c06f02ffd9d6c911b6453cc8b717083ab6417319a6ec5c3bb39ed0baf331',
  },
  'turboshake256': {
    empty: '367a329dafea871c7802ec67f905ae13c57695dc2c6663c61035f59a18f8e7db' +
           '11edc0e12e91ea60eb6b32df06dd7f002fbafabb6e13ec1cc20d995547600db0',
    short: 'b47aa0a5b76caf9b10cfaeff036df0cdb86362d2bd036a2fee0cd0d74e79279c' +
           'b9c57a70da1e3dd9e126a469857ba4c82b0efb3ae06d1a3781a6f102c3eb3a1d',
    medium: '7fa19fd828762d2dba6eea8407d1fb04302b5a4f1ca3d00b3672c1e3b3331d18' +
            '925b7ec380f3f04673a164dab04d2a0c5c12818046284c38d286645741a8aa3e',
    long: '12d0b90c08f588710733cc07f0a2d6ab0795a4a24904c111062226fcd9d5dcb2' +
          '1d6b5b848c9aebbcab221f031e9b4ea71e099ec785e822b1b83e73d0750ca1a7',
  },
  'kt128': {
    empty: '1ac2d450fc3b4205d19da7bfca1b37513c0803577ac7167f06fe2ce1f0ef39e5',
    short: '4719a2ac1dc1c592521cf201df3f476ea496fe461abe9a2604527f6bec047579',
    medium: '00f3add71679681720b925416953897ac62cfae97060dd5f2e1641a076580cc9',
    long: 'c05805c2736deb4be3fca6e3717b9af0aa18ceeaaeeab66b328a3ffebf0a814d',
  },
  'kt256': {
    empty: 'b23d2e9cea9f4904e02bec06817fc10ce38ce8e93ef4c89e6537076af8646404' +
           'e3e8b68107b8833a5d30490aa33482353fd4adc7148ecb782855003aaebde4a9',
    short: '6709e5e312f2dee4547ecb0ab7d42728ba57985983731afbd6c2a0676c522274' +
           'cf9153064ee07982129d3f58d4dbe00050eb28b392559bdb020aca302b7a28cb',
    medium: '9078b6ff78e9c4b3c8ff49e5b9f337b36cc6d6749d23985035886d993db69f7e' +
            '05fea97125e0889130da09fc5837761f7793e3e44d85be1ee1f6af7f4a1f50cb',
    long: '41f83b7c7d02fc6d98f1fa1474d765caff4673f90cd7204894d7da72d97403b6' +
          '2fe5c4bae2bf0ce3dcd51e80c98bd25ce5fe54040259d9466b67f1517dac0712',
  },
};

function buildAlg(name) {
  const lower = name.toLowerCase();
  if (lower.startsWith('turboshake')) {
    const outputLength = lower === 'turboshake128' ? 256 : 512;
    return { name, outputLength };
  }
  if (lower.startsWith('kt')) {
    const outputLength = lower === 'kt128' ? 256 : 512;
    return { name, outputLength };
  }
  return name;
}

async function testDigest(size, alg) {
  const digest = await subtle.digest(
    alg,
    Buffer.from(kSourceData[size], 'hex'));

  assert.strictEqual(
    Buffer.from(digest).toString('hex'),
    kDigestedData[(alg.name || alg).toLowerCase()][size]);
}

// Known-answer tests
(async function() {
  const variations = [];
  Object.keys(kSourceData).forEach((size) => {
    Object.keys(kDigestedData).forEach((alg) => {
      const upCase = alg.toUpperCase();
      const downCase = alg.toLowerCase();
      const mixedCase = upCase.slice(0, 1) + downCase.slice(1);

      variations.push(testDigest(size, buildAlg(upCase)));
      variations.push(testDigest(size, buildAlg(downCase)));
      variations.push(testDigest(size, buildAlg(mixedCase)));
    });
  });

  await Promise.all(variations);
})().then(common.mustCall());

// Edge cases: zero-length output rejects
(async () => {
  await assert.rejects(
    subtle.digest({ name: 'TurboSHAKE128', outputLength: 0 }, Buffer.alloc(1)),
    {
      name: 'OperationError',
      message: 'Invalid TurboShakeParams outputLength',
    });

  await assert.rejects(
    subtle.digest({ name: 'KT128', outputLength: 0 }, Buffer.alloc(1)),
    {
      name: 'OperationError',
      message: 'Invalid KangarooTwelveParams outputLength',
    });
})().then(common.mustCall());

// Edge case: non-byte-aligned outputLength rejects
(async () => {
  await assert.rejects(
    subtle.digest({ name: 'TurboSHAKE128', outputLength: 7 }, Buffer.alloc(1)),
    {
      name: 'OperationError',
      message: 'Invalid TurboShakeParams outputLength',
    });

  await assert.rejects(
    subtle.digest({ name: 'KT128', outputLength: 7 }, Buffer.alloc(1)),
    {
      name: 'OperationError',
      message: 'Invalid KangarooTwelveParams outputLength',
    });
})().then(common.mustCall());

// TurboSHAKE domain separation byte
(async () => {
  // Domain separation 0x07 should produce different output than default 0x1F
  const [d07, d1f] = await Promise.all([
    subtle.digest(
      { name: 'TurboSHAKE128', outputLength: 256, domainSeparation: 0x07 },
      Buffer.alloc(0)),
    subtle.digest(
      { name: 'TurboSHAKE128', outputLength: 256 },
      Buffer.alloc(0)),
  ]);
  assert.notDeepStrictEqual(
    new Uint8Array(d07),
    new Uint8Array(d1f));

  // Verify D=0x07 against known vector
  assert.strictEqual(
    Buffer.from(d07).toString('hex'),
    '5a223ad30b3b8c66a243048cfced430f54e7529287d15150b973133adfac6a2f');
})().then(common.mustCall());

// KT128 with customization string
(async () => {
  const digest = await subtle.digest(
    { name: 'KT128', outputLength: 256, customization: Buffer.from('test') },
    Buffer.from('hello'));
  assert(digest instanceof ArrayBuffer);
  assert.strictEqual(digest.byteLength, 32);
})().then(common.mustCall());

// TurboSHAKE domain separation out of range
(async () => {
  await assert.rejects(
    subtle.digest(
      { name: 'TurboSHAKE128', outputLength: 256, domainSeparation: 0x00 },
      Buffer.alloc(0)),
    {
      name: 'OperationError',
    });
  await assert.rejects(
    subtle.digest(
      { name: 'TurboSHAKE128', outputLength: 256, domainSeparation: 0x80 },
      Buffer.alloc(0)),
    {
      name: 'OperationError',
    });
})().then(common.mustCall());
