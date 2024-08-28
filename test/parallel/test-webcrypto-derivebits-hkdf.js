'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

function getDeriveKeyInfo(name, length, hash, ...usages) {
  return [{ name, length, hash }, usages];
}

const kDerivedKeyTypes = [
  ['AES-CBC', 128, undefined, 'encrypt', 'decrypt'],
  ['AES-CBC', 256, undefined, 'encrypt', 'decrypt'],
  ['AES-CTR', 128, undefined, 'encrypt', 'decrypt'],
  ['AES-CTR', 256, undefined, 'encrypt', 'decrypt'],
  ['AES-GCM', 128, undefined, 'encrypt', 'decrypt'],
  ['AES-GCM', 256, undefined, 'encrypt', 'decrypt'],
  ['AES-KW', 128, undefined, 'wrapKey', 'unwrapKey'],
  ['AES-KW', 256, undefined, 'wrapKey', 'unwrapKey'],
  ['HMAC', 256, 'SHA-1', 'sign', 'verify'],
  ['HMAC', 256, 'SHA-256', 'sign', 'verify'],
  ['HMAC', 256, 'SHA-384', 'sign', 'verify'],
  ['HMAC', 256, 'SHA-512', 'sign', 'verify'],
  ['HMAC', 256, 'SHA3-256', 'sign', 'verify'],
  ['HMAC', 256, 'SHA3-384', 'sign', 'verify'],
  ['HMAC', 256, 'SHA3-512', 'sign', 'verify'],
];

const kDerivedKeys = {
  short: '5040737377307264',
  long: '55736572732073686f756c64207069636b206c6f6e6720706173737068726' +
          '173657320286e6f74207573652073686f72742070617373776f7264732921',
  empty: ''
};

const kSalts = {
  normal: '536f6469756d2043686c6f7269646520636f6d706f756e64',
  empty: ''
};

const kInfos = {
  normal: '484b444620657874726120696e666f',
  empty: ''
};

const kDerivations = {
  short: {
    normal: {
      'SHA-384': {
        normal: '19ba74368e6b993390f27fe9a7d02bc3' +
                '38173f72be71a19fc744fcdb3fd4b84b',
        empty: '97601f4e0c53a5d3f3a2810099bc6820' +
               'ec50083434769b59fc24a417a9543734'
      },
      'SHA-512': {
        normal: '4bbd6db2435fb696157f6089c977c3c7' +
                '3f3eac5ef3dd6baae604cb53bfbb153e',
        empty: '2f3157e7fe0c10b01298c8f0886a90ed' +
               'cf80abdef5dbc1df2b1482532b52b934'
      },
      'SHA-1': {
        normal: '05ad22ed2138c9600e4d9e2725ded301' +
                'f5d287fbfb5702f999bc6536d3edef98',
        empty: 'd51b6fb7e599ca30c5ee264593e4b85f' +
               '2220c7c3ab003157bff8cb4f369c7560'
      },
      'SHA-256': {
        normal: '2af5901e28849c28443857386aa1ac3b' +
                'b127e92631c1c051482d6690941772b4',
        empty: '9e4b719033742101e90f1ad61e2ff3b4' +
               '256863667296d74389f1f02af2c4e6a6'
      },
      'SHA3-256': {
        normal: '386b0693d7a58c4ddf01b49bfbbd2fa87c6f911991543995170ba20ed28df599',
        empty: 'd029bc828b6c6c8bb16ce3d25f5058f19c7d2517745e11c5d65c6d242e82e47f',
      },
      'SHA3-384': {
        normal: '8c3b72e659bad40bcd14bdc1f7c3836059d24253795ab046a272973fd0456508',
        empty: '3211ff4c676f761494c1ca2683d2d4662fe1d770ae5c58ebf6af6acb181c7d71',
      },
      'SHA3-512': {
        normal: '5588c5c70cb3dd2f95323da2e9d5f299ca99c301d920a499330c449d21c645cd',
        empty: '2c944b916c2751a71a1b5e57fcb487939c624335683995770b9f7cc7cbbb21f0',
      },
    },
    empty: {
      'SHA-384': {
        normal: 'fb482ff22c4f8d466c4dfe6e29f2cc2e' +
                'cdabf5884328fbf08a738fd945f166cb',
        empty: '1e023c17b340533ceaef39230cb8b3bb' +
               'dbf663a13d6075d0dd326c049478fba5'
      },
      'SHA-512': {
        normal: 'f17b5bdcd8d7d3d4601036a19436317d' +
                '1644f9a4e0956efc0e372b83acdacfdb',
        empty: 'c7b474942f31f83faf5d14731802b1bd' +
               '49478549cb3a8f3dbfedc4d3209cf5b6'
      },
      'SHA-1': {
        normal: 'c126f1e6f25a9de42cf7d427059a52ed' +
                '9601f29a5815cbfbc64bc7f668c6a341',
        empty: '3215c3f08de70549b051b7033745a818' +
               '4f8cbaa6b1735330d2bcb6b16f4642ef'
      },
      'SHA-256': {
        normal: '733c8b6bcfac875c7f08982a6e3ffb56' +
                '0acea6f165476eb83460b9353ed41dfe',
        empty: 'c8e12774135305c9147f2cc4766e5ead' +
               '25d8f457b9a1953d52677361ced558fb'
      },
      'SHA3-256': {
        normal: '9befc557f5baf4075b5fb38c014b41b92ab7534150baf64201069e8807d0e83d',
        empty: '54d1fa1aa7cad99dab0622b772170e775c103756183bac36a228fd817a98a3f6',
      },
      'SHA3-384': {
        normal: '46b54c015e368677edf7ac16963bccd9d2ba8246eef0e8beb04d8d188774b91b',
        empty: '46eb0b2649bb0f605d70e4818ffc8176ee1be9782396e69fb4d0fd7cfe902b55',
      },
      'SHA3-512': {
        normal: 'aa4375c82b5d7a3cac88a0423250b3882f140c253e98e8e7a0f6055b0908e4c2',
        empty: '6613003f98602ddb53ac35f5aa256c9f5279d50ee65bb08fdf2ecf65cc5df27f',
      },
    }
  },
  long: {
    normal: {
      'SHA-384': {
        normal: 'f91571b521f7eef13e573aa46378659e' +
                'f3b7f36ffdd1bb055db2cd77d260c467',
        empty: '68af1c2cf6b9370d2054344798bdbb18' +
               '47ccf407b7652b793dd136d4640e0348'
      },
      'SHA-512': {
        normal: '710aae2fdf889e45fe0fb995b2c26b33' +
                'eb988650ec0faef167028a7a6ccb3638',
        empty: 'e5de568081c71e562750829871c34275' +
               '8104765ed6f306f0613c9d4bb336f2aa'
      },
      'SHA-1': {
        normal: '7f957edcbce3cb0b70566e1eb60efd1e' +
                '405a13304c661d3663778109bf06899c',
        empty: '3062f3cf1a730b9cef51f02c1dfac85e' +
               'd91e4b0065eb50ca9fd8b0107e728733'
      },
      'SHA-256': {
        normal: '31b7d68530a863e717c081ca6917b686' +
                '50b3dd9a29f30606e2cad199bec14d13',
        empty: 'e579d1f9e7f08e6f990ffcfcce1ed201' +
               'c5e37e62cdf606f0ba4aca80427fbc44'
      },
      'SHA3-256': {
        normal: '24f38fd1905554b7cbf8395cc3976292d11ce24a0b3131da0fd4b109832d27e3',
        empty: '33d0a5151c0f52e4bb7fb67cf7a17063127624dc3e685903f49ebb07872084d1',
      },
      'SHA3-384': {
        normal: '15777581a1ea81ad0baac8a97d954df4142f7260d9e8351aa7f6ef6de2d04632',
        empty: 'ada4da4e28dc971633a8760b265b3019db57baf17e7bf7e13cf78b1a676f6d44',
      },
      'SHA3-512': {
        normal: '621e4602b07fcba55ed6b976a8bef513b0f7c4ad0c546e0f852993051d887408',
        empty: 'f1292af65b05c86cf7146b739bc65785c707450316f3207ee54a3f596a7d0f7b',
      },
    },
    empty: {
      'SHA-384': {
        normal: '619eb6f9287395bbd5ed6a67c968465a' +
                'd82b6c559f3c38b604bbb08f58320b03',
        empty: 'ff447b423d83fe76836c32337228b56b' +
               '5bd9bf68d58e7dca4b7cca842a45e11a'
      },
      'SHA-512': {
        normal: '133e8a7f7ff433690cc88432c2a338c2' +
                '77e5c13756ff878f46753fe6a564e3e5',
        empty: 'de54f7eec80c9cc66d349fc987f80d46' +
               '1db2ef4ff4e18505d28bd80cb42c7d76'
      },
      'SHA-1': {
        normal: 'adb93cdbce79b7d51159b6c0131a2b62' +
                'f23828d26acd685e34c06535e6f77496',
        empty: '47710d2a7507e05a1ddcc87a7c2f9061' +
               '77a266efb9e622510cccb3713cd08d58'
      },
      'SHA-256': {
        normal: 'a401d7c9158a29e5c7193ab9730f0748' +
                '851cc5baadb42cad024b6290fe213436',
        empty: 'b4f7e7557674d501cbfbc0148ad800c0' +
               '750189fe295a2aca5e1bf4122c85edf9'
      },
      'SHA3-256': {
        normal: 'fe32459f7339dd2e8df6c6fc874ed9e81e3b7aad669edad9b71196f53ed95b12',
        empty: '04519be1eb94079c91306cc5b21946b3de6a78ad35ec83d4f4a37bafbda678d7',
      },
      'SHA3-384': {
        normal: 'a474e8289cb4a0511e90b87eaf9ec29cadd74d4c1f2ee1fb8cb5f7d08f91a379',
        empty: '726c8c4b39083a7d5755604d3a67e9aa6139db00c08028ac9e69f7fb1525bf1d',
      },
      'SHA3-512': {
        normal: 'c7a7f5004d1d595c6896498c169642ac24b946e13296ff53e12b534962a88675',
        empty: '7b543480b5696932551abb3100d72e05c18f57fbb63aa44fe020bef1eec3555c',
      },
    }
  },
};

async function setupBaseKeys() {
  const promises = [];

  const baseKeys = {};
  const noBits = {};
  const noKey = {};
  let wrongKey = null;

  Object.keys(kDerivedKeys).forEach((size) => {
    const keyData = Buffer.from(kDerivedKeys[size], 'hex');
    promises.push(
      subtle.importKey(
        'raw',
        keyData,
        { name: 'HKDF' },
        false,
        ['deriveKey', 'deriveBits'])
        .then(
          (baseKey) => baseKeys[size] = baseKey,
          (err) => assert.ifError(err)));

    promises.push(
      subtle.importKey(
        'raw',
        keyData,
        { name: 'HKDF' },
        false, ['deriveBits'])
        .then(
          (baseKey) => noKey[size] = baseKey,
          (err) => assert.ifError(err)));

    promises.push(
      subtle.importKey(
        'raw',
        keyData,
        { name: 'HKDF' },
        false, ['deriveKey'])
        .then(
          (baseKey) => noBits[size] = baseKey,
          (err) => assert.ifError(err)));
  });

  promises.push(
    subtle.generateKey(
      {
        name: 'ECDH',
        namedCurve: 'P-521'
      },
      false,
      ['deriveKey', 'deriveBits'])
      .then(
        (baseKey) => wrongKey = baseKey.privateKey,
        (err) => assert.ifError(err)));

  await Promise.all(promises);

  return {
    baseKeys,
    noBits,
    noKey,
    wrongKey
  };
}

async function testDeriveBits(
  baseKeys,
  size,
  saltSize,
  hash,
  infoSize) {
  const algorithm = {
    name: 'HKDF',
    salt: Buffer.from(kSalts[saltSize], 'hex'),
    info: Buffer.from(kInfos[infoSize], 'hex'),
    hash
  };

  const bits = await subtle.deriveBits(
    algorithm,
    baseKeys[size],
    256);

  assert(bits instanceof ArrayBuffer);
  assert.strictEqual(
    Buffer.from(bits).toString('hex'),
    kDerivations[size][saltSize][hash][infoSize]);
}

async function testDeriveBitsBadLengths(
  baseKeys,
  size,
  saltSize,
  hash,
  infoSize) {
  const algorithm = {
    name: 'HKDF',
    salt: Buffer.from(kSalts[saltSize], 'hex'),
    info: Buffer.from(kInfos[infoSize], 'hex'),
    hash
  };

  return Promise.all([
    assert.rejects(
      subtle.deriveBits(algorithm, baseKeys[size], undefined), {
        name: 'OperationError',
      }),
    assert.rejects(
      subtle.deriveBits(algorithm, baseKeys[size], null), {
        message: 'length cannot be null',
        name: 'OperationError',
      }),
    assert.rejects(
      subtle.deriveBits(algorithm, baseKeys[size]), {
        message: 'length cannot be null',
        name: 'OperationError',
      }),
    assert.rejects(
      subtle.deriveBits(algorithm, baseKeys[size], 15), {
        message: /length must be a multiple of 8/,
        name: 'OperationError',
      }),
  ]);
}

async function testDeriveBitsBadHash(
  baseKeys,
  size,
  saltSize,
  hash,
  infoSize) {
  const salt = Buffer.from(kSalts[saltSize], 'hex');
  const info = Buffer.from(kInfos[infoSize], 'hex');
  const algorithm = { name: 'HKDF', salt, info };

  return Promise.all([
    assert.rejects(
      subtle.deriveBits(
        {
          ...algorithm,
          hash: hash.replace('-', '')
        }, baseKeys[size], 256), {
        message: /Unrecognized algorithm name/,
        name: 'NotSupportedError',
      }),
    assert.rejects(
      subtle.deriveBits(
        {
          ...algorithm,
          hash: 'PBKDF2'
        },
        baseKeys[size], 256), {
        name: 'NotSupportedError',
      }),
  ]);
}

async function testDeriveBitsBadUsage(
  noBits,
  size,
  saltSize,
  hash,
  infoSize) {
  const algorithm = {
    name: 'HKDF',
    salt: Buffer.from(kSalts[saltSize], 'hex'),
    info: Buffer.from(kInfos[infoSize], 'hex'),
    hash
  };

  return assert.rejects(
    subtle.deriveBits(algorithm, noBits[size], 256), {
      message: /baseKey does not have deriveBits usage/
    });
}

async function testDeriveBitsMissingSalt(
  baseKeys,
  size,
  saltSize,
  hash,
  infoSize) {
  const algorithm = {
    name: 'HKDF',
    info: Buffer.from(kInfos[infoSize], 'hex'),
    hash
  };

  return assert.rejects(
    subtle.deriveBits(algorithm, baseKeys[size], 0), {
      code: 'ERR_MISSING_OPTION'
    });
}

async function testDeriveBitsMissingInfo(
  baseKeys,
  size,
  saltSize,
  hash,
  infoSize) {
  const algorithm = {
    name: 'HKDF',
    salt: Buffer.from(kSalts[infoSize], 'hex'),
    hash
  };

  return assert.rejects(
    subtle.deriveBits(algorithm, baseKeys[size], 0), {
      code: 'ERR_MISSING_OPTION'
    });
}

async function testBitsWrongKeyType(
  wrongKey,
  saltSize,
  hash,
  infoSize) {
  const algorithm = {
    name: 'HKDF',
    salt: Buffer.from(kSalts[saltSize], 'hex'),
    info: Buffer.from(kInfos[infoSize], 'hex'),
    hash
  };

  return assert.rejects(
    subtle.deriveBits(algorithm, wrongKey, 256), {
      message: /Key algorithm mismatch/
    });
}

async function testDeriveKey(
  baseKeys,
  size,
  saltSize,
  hash,
  infoSize,
  keyType,
  usages) {
  const algorithm = {
    name: 'HKDF',
    salt: Buffer.from(kSalts[saltSize], 'hex'),
    info: Buffer.from(kInfos[infoSize], 'hex'),
    hash
  };

  const key = await subtle.deriveKey(
    algorithm,
    baseKeys[size],
    keyType,
    true,
    usages);

  const bits = await subtle.exportKey('raw', key);

  assert.strictEqual(
    Buffer.from(bits).toString('hex'),
    kDerivations[size][saltSize][hash][infoSize].slice(0, keyType.length / 4));
}

async function testDeriveKeyBadHash(
  baseKeys,
  size,
  saltSize,
  hash,
  infoSize,
  keyType,
  usages) {
  const salt = Buffer.from(kSalts[saltSize], 'hex');
  const info = Buffer.from(kInfos[infoSize], 'hex');
  const algorithm = { name: 'HKDF', salt, info };

  return Promise.all([
    assert.rejects(
      subtle.deriveKey(
        {
          ...algorithm,
          hash: hash.replace('-', '')
        },
        baseKeys[size],
        keyType,
        true,
        usages),
      { name: 'NotSupportedError' }),
    assert.rejects(
      subtle.deriveKey(
        {
          ...algorithm,
          hash: 'PBKDF2'
        },
        baseKeys[size],
        keyType,
        true,
        usages),
      { name: 'NotSupportedError' }),
  ]);
}

async function testDeriveKeyBadUsage(
  noKey,
  size,
  saltSize,
  hash,
  infoSize,
  keyType,
  usages) {
  const algorithm = {
    name: 'HKDF',
    salt: Buffer.from(kSalts[saltSize], 'hex'),
    info: Buffer.from(kInfos[infoSize], 'hex'),
    hash
  };

  return assert.rejects(
    subtle.deriveKey(algorithm, noKey[size], keyType, true, usages), {
      message: /baseKey does not have deriveKey usage/
    });
}

async function testWrongKeyType(
  wrongKey,
  saltSize,
  hash,
  infoSize,
  keyType,
  usages
) {
  const algorithm = {
    name: 'HKDF',
    salt: Buffer.from(kSalts[saltSize], 'hex'),
    info: Buffer.from(kInfos[infoSize], 'hex'),
    hash
  };

  return assert.rejects(
    subtle.deriveKey(algorithm, wrongKey, keyType, true, usages), {
      message: /Key algorithm mismatch/
    });
}

(async function() {
  const {
    baseKeys,
    noBits,
    noKey,
    wrongKey,
  } = await setupBaseKeys();

  const variations = [];

  Object.keys(kDerivations).forEach((size) => {
    Object.keys(kDerivations[size]).forEach((saltSize) => {
      Object.keys(kDerivations[size][saltSize]).forEach((hash) => {
        Object.keys(kDerivations[size][saltSize][hash]).forEach(
          async (infoSize) => {
            const args = [baseKeys, size, saltSize, hash, infoSize];
            variations.push(testDeriveBits(...args));
            variations.push(testDeriveBitsBadLengths(...args));
            variations.push(testDeriveBitsMissingSalt(...args));
            variations.push(testDeriveBitsMissingInfo(...args));
            variations.push(testDeriveBitsBadHash(...args));
            variations.push(
              testDeriveBitsBadUsage(
                noBits,
                size,
                saltSize,
                hash,
                infoSize));
            variations.push(
              testBitsWrongKeyType(
                wrongKey,
                saltSize,
                hash,
                infoSize));

            kDerivedKeyTypes.forEach((keyType) => {
              const keyArgs = getDeriveKeyInfo(...keyType);
              variations.push(testDeriveKey(...args, ...keyArgs));
              variations.push(testDeriveKeyBadHash(...args, ...keyArgs));
              variations.push(testDeriveKeyBadUsage(
                noKey, size, saltSize, hash, infoSize, ...keyArgs));
              variations.push(
                testWrongKeyType(
                  wrongKey,
                  saltSize,
                  hash,
                  infoSize,
                  ...keyArgs));
            });
          });
      });
    });
  });

  await Promise.all(variations);

})().then(common.mustCall());

// https://github.com/w3c/webcrypto/pull/380
{
  crypto.subtle.importKey('raw', new Uint8Array(0), 'HKDF', false, ['deriveBits']).then((key) => {
    return crypto.subtle.deriveBits({
      name: 'HKDF',
      hash: { name: 'SHA-256' },
      info: new Uint8Array(0),
      salt: new Uint8Array(0),
    }, key, 0);
  }).then((bits) => {
    assert.deepStrictEqual(bits, new ArrayBuffer(0));
  })
  .then(common.mustCall());
}
