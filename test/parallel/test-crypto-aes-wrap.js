'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}

// Tests that the AES wrap and unwrap functions are working correctly.

const assert = require('assert');
const crypto = require('crypto');

const ivShort = Buffer.from('3fd838af', 'hex');
const ivLong = Buffer.from('3fd838af4093d749', 'hex');
const key1 = Buffer.from('b26f309fbe57e9b3bb6ae5ef31d54450', 'hex');
const key2 = Buffer.from('40978085d68091f7dfca0d7dfc7a5ee76d2cc7f2f345a304', 'hex');
const key3 = Buffer.from('29c9eab5ed5ad44134a1437fe2e673b4d88a5b7c72e68454fea08721392b7323', 'hex');

[
  {
    algorithm: 'aes128-wrap',
    key: key1,
    iv: ivLong,
    text: '12345678123456781234567812345678'
  },
  {
    algorithm: 'id-aes128-wrap-pad',
    key: key1,
    iv: ivShort,
    text: '12345678123456781234567812345678123'
  },
  {
    algorithm: 'aes192-wrap',
    key: key2,
    iv: ivLong,
    text: '12345678123456781234567812345678'
  },
  {
    algorithm: 'id-aes192-wrap-pad',
    key: key2,
    iv: ivShort,
    text: '12345678123456781234567812345678123'
  },
  {
    algorithm: 'aes256-wrap',
    key: key3,
    iv: ivLong,
    text: '12345678123456781234567812345678'
  },
  {
    algorithm: 'id-aes256-wrap-pad',
    key: key3,
    iv: ivShort,
    text: '12345678123456781234567812345678123'
  },
].forEach(({ algorithm, key, iv, text }) => {
  if (!crypto.getCiphers().includes(algorithm)) {
    common.printSkipMessage(`Skipping unsupported ${algorithm} test case`);
    return;
  }

  const cipher = crypto.createCipheriv(algorithm, key, iv);
  const decipher = crypto.createDecipheriv(algorithm, key, iv);
  const msg = decipher.update(cipher.update(text, 'utf8'), 'buffer', 'utf8');
  assert.strictEqual(msg, text, `${algorithm} test case failed`);
});

const kwIV = Buffer.alloc(8, 0xa6);
const kwpIV = Buffer.from('a65959a6', 'hex');

// NIST SP 800-38F known-answer vectors.
[
  {
    algorithm: 'aes-128-wrap',
    key: '000102030405060708090a0b0c0d0e0f',
    plaintext: '00112233445566778899aabbccddeeff',
    ciphertext: '1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5',
    iv: kwIV,
  },
  {
    algorithm: 'aes-192-wrap',
    key: '000102030405060708090a0b0c0d0e0f1011121314151617',
    plaintext: '00112233445566778899aabbccddeeff',
    ciphertext: '96778b25ae6ca435f92b5b97c050aed2468ab8a17ad84e5d',
    iv: kwIV,
  },
  {
    algorithm: 'aes-256-wrap',
    key: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    plaintext: '00112233445566778899aabbccddeeff',
    ciphertext: '64e8c3f9ce0f5ba263e9777905818a2a93c8191e7d6e8ae7',
    iv: kwIV,
  },
  {
    algorithm: 'aes-128-wrap-pad',
    key: '6decf10a1caf8e3b80c7a4be8c9c84e8',
    plaintext: '49',
    ciphertext: '01a7d657fc4a5b216f261cca4d052c2b',
    iv: kwpIV,
  },
  {
    algorithm: 'aes-192-wrap-pad',
    key: '9ca11078baebc1597a68ce2fe3fc79a201626575252b8860',
    plaintext: '76',
    ciphertext: '866bc0ae30e290bb20a0dab31a6e7165',
    iv: kwpIV,
  },
  {
    algorithm: 'aes-256-wrap-pad',
    key: '95da2700ca6fd9a52554ee2a8df1386f5b94a1a60ed8a4aef60a8d61ab5f225a',
    plaintext: 'd1',
    ciphertext: '06ba7ae6f3248cfdcf267507fa001bc4',
    iv: kwpIV,
  },
  {
    algorithm: 'aes-128-wrap-inv',
    key: 'e88ba734ea243480a6129366753b58eb',
    plaintext: 'd140ac16a44c1c2b3f47037ea8898a3e',
    ciphertext: '600861ee14320006f0ae55c46d5e1ebf3303751df7f038df',
    iv: kwIV,
  },
  {
    algorithm: 'aes-192-wrap-inv',
    key: '370c715135b44eb3773b1aff833bcd28b59aee866d4a36b3',
    plaintext: 'eae0f60f1cf33d5b75869e84c764a04e',
    ciphertext: 'ea4ba4add8add19950ca491d109ffa08f90312693055677a',
    iv: kwIV,
  },
  {
    algorithm: 'aes-256-wrap-inv',
    key: 'de982f7c871f78e37462e2f48a62eecb2da81a10799c6ebf2bee8c786b624b0e',
    plaintext: 'ecafc437d9f1643c7645c2416c14c003',
    ciphertext: 'aec02ddb3f6de1f99103c6042dfc9001eb3cf56d9c2a11f7',
    iv: kwIV,
  },
  {
    algorithm: 'aes-128-wrap-pad-inv',
    key: '1c321a356b0ee25e30de2d618c1facbe',
    plaintext: '42',
    ciphertext: '3ddf22da3080a1a5252574c76f833790',
    iv: kwpIV,
  },
  {
    algorithm: 'aes-192-wrap-pad-inv',
    key: 'fe3fe235bb36dcf03f01cbf32cc98a3abf10ab3d608d3b30',
    plaintext: '1d2b7fc29991bafaf7',
    ciphertext: 'c11afb3c0de263dfb9b672a5f81fe0b9acfe9c407691f332',
    iv: kwpIV,
  },
  {
    algorithm: 'aes-256-wrap-pad-inv',
    key: '148a3fa618a6998c30b9f0f67922354a3747f2fa2e4d2e0b7af9582d6f548fee',
    plaintext: '441125592acf9e5208dcd558a7ac0034d15530dbad7a2913963da0cbf60aa3',
    ciphertext: '23f26a9476829885055694062c89b86399e8d6125509c9e88bb0a5b5113f4bfc8d34a62cba3c9eee',
    iv: kwpIV,
  },
].forEach(({ algorithm, key, plaintext, ciphertext, iv }) => {
  if (!crypto.getCiphers().includes(algorithm)) {
    common.printSkipMessage(`Skipping unsupported ${algorithm} test case`);
    return;
  }

  const keyBuffer = Buffer.from(key, 'hex');
  const plaintextBuffer = Buffer.from(plaintext, 'hex');
  const expected = Buffer.from(ciphertext, 'hex');
  const cipher = crypto.createCipheriv(algorithm, keyBuffer, iv);
  const actual = Buffer.concat([
    cipher.update(plaintextBuffer),
    cipher.final(),
  ]);
  assert.deepStrictEqual(actual, expected, `${algorithm} wrap failed`);

  const decipher = crypto.createDecipheriv(algorithm, keyBuffer, iv);
  const unwrapped = Buffer.concat([
    decipher.update(actual),
    decipher.final(),
  ]);
  assert.deepStrictEqual(
    unwrapped, plaintextBuffer, `${algorithm} unwrap failed`);
});

{
  const algorithm = crypto.getCiphers().includes('aes-128-wrap-inv') ?
    'aes-128-wrap-inv' : 'aes128-wrap';
  if (!crypto.getCiphers().includes(algorithm)) {
    common.printSkipMessage(`Skipping unsupported ${algorithm} state tests`);
  } else {
    const key = Buffer.from('e88ba734ea243480a6129366753b58eb', 'hex');
    const iv = Buffer.alloc(8, 0xa6);
    const plaintextParts = [Buffer.alloc(16), Buffer.alloc(16, 1)];
    const wrappedParts = plaintextParts.map((plaintext) => {
      const cipher = crypto.createCipheriv(algorithm, key, iv);
      return Buffer.concat([cipher.update(plaintext), cipher.final()]);
    });

    for (const [create, inputParts] of [
      [crypto.createCipheriv, plaintextParts],
      [crypto.createDecipheriv, wrappedParts],
    ]) {
      const withoutUpdate = create(algorithm, key, iv);
      assert.throws(() => withoutUpdate.final(), /Unsupported state/);

      const multipleUpdates = create(algorithm, key, iv);
      multipleUpdates.update(inputParts[0]);
      assert.throws(() => multipleUpdates.update(inputParts[1]),
                    /Trying to add data in unsupported state/);
    }
  }
}
