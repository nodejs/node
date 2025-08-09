'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { Buffer } = require('buffer');
const { subtle } = globalThis.crypto;
const { createHash, getHashes } = require('crypto');

const kTests = [
  ['SHA-1', ['sha1'], 160],
  ['SHA-256', ['sha256'], 256],
  ['SHA-384', ['sha384'], 384],
  ['SHA-512', ['sha512'], 512],
];

if (!process.features.openssl_is_boringssl) {
  kTests.push(
    [{ name: 'cSHAKE128', length: 256 }, ['shake128', { outputLength: 256 >> 3 }], 256],
    [{ name: 'cSHAKE256', length: 512 }, ['shake256', { outputLength: 512 >> 3 }], 512],
    ['SHA3-256', ['sha3-256'], 256],
    ['SHA3-384', ['sha3-384'], 384],
    ['SHA3-512', ['sha3-512'], 512],
  );
} else {
  common.printSkipMessage('Skipping unsupported test cases');
}

// Empty hash just works, not checking result
subtle.digest('SHA-512', Buffer.alloc(0))
  .then(common.mustCall());

// TODO(@jasnell): Need to move this particular test to pummel
// // Careful, this is an expensive operation because of both the memory
// // allocation and the cost of performing the hash on such a large
// // input.
// subtle.digest('SHA-512', new ArrayBuffer(2 ** 31 - 1))
//   .then(common.mustCall());

// TODO(@jasnell): Change max to 2 ** 31 - 1
// assert.rejects(subtle.digest('SHA-512', new ArrayBuffer(kMaxLength + 1)), {
//   code: 'ERR_OUT_OF_RANGE'
// });

const kData = (new TextEncoder()).encode('hello');
(async function() {
  await Promise.all(kTests.map(async (test) => {
    // Get the digest using the legacy crypto API
    const checkValue =
      createHash.apply(createHash, test[1]).update(kData).digest().toString('hex');

    // Get the digest using the SubtleCrypto API
    const values = Promise.all([
      subtle.digest(test[0], kData),
      subtle.digest(test[0], kData.buffer),
      subtle.digest(test[0], new DataView(kData.buffer)),
      subtle.digest(test[0], Buffer.from(kData)),
    ]);

    // subtle.digest copies the input data, so changing it
    // while we're waiting on the Promises should never
    // cause the test to fail.
    kData[0] = 0x1;
    kData[2] = 0x2;
    kData[4] = 0x3;

    // Compare that the legacy crypto API and SubtleCrypto API
    // produce the same results
    (await values).forEach((v) => {
      assert(v instanceof ArrayBuffer);
      assert.strictEqual(checkValue, Buffer.from(v).toString('hex'));
    });
  }));
})().then(common.mustCall());

Promise.all([1, null, undefined].map((i) =>
  assert.rejects(subtle.digest(i, Buffer.alloc(0)), {
    message: /Unrecognized algorithm name/,
    name: 'NotSupportedError',
  })
)).then(common.mustCall());

assert.rejects(subtle.digest('', Buffer.alloc(0)), {
  message: /Unrecognized algorithm name/,
  name: 'NotSupportedError',
}).then(common.mustCall());

Promise.all([1, [], {}, null, undefined].map((i) =>
  assert.rejects(subtle.digest('SHA-256', i), {
    code: 'ERR_INVALID_ARG_TYPE'
  })
)).then(common.mustCall());

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

const kDigestedData = {
  'sha-1': {
    empty: 'da39a3ee5e6b4b0d3255bfef95601890afd80709',
    short: 'c91318cdf2396a015e3f4e6a86a0ba65b8635944',
    medium: 'e541060870eb16bf33b68e51f513526893986729',
    long: '3098b50037ecd02ebd657653b2bfa01eee27a2ea'
  },
  'sha-256': {
    empty: 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
    short: 'a2831186984792c7d32d59c89740687f19addc1b959e71a1cc538a3b7ed843f2',
    medium: '535367877ef014d7fc717e5cb7843e59b61aee62c7029cec7ec6c12fd924e0e4',
    long: '14cdea9dc75f5a6274d9fc1e64009912f1dcd306b48fe8e9cf122de671571781'
  },
  'sha-384': {
    empty: '38b060a751ac96384cd9327eb1b1e36a21fdb71114b' +
           'e07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2' +
           'f14898b95b',
    short: '6bf5ea6524d1cddc43f7cf3b56ee059227404a2f538' +
           'f022a3db7447a782c06c1ed05e8ab4f5edc17f37114' +
           '40dfe97731',
    medium: 'cbc2c588fe5b25f916da28b4e47a484ae6fc1fe490' +
            '2dd5c9939a6bfd034ab3b48b39087436011f6a9987' +
            '9d279540e977',
    long: '49f4fdb3981968f97d57370f85345067cd5296a97dd1' +
          'a18e06911e756e9608492529870e1ad130998d57cbfb' +
          'b7c1d09e'
  },
  'sha-512': {
    empty: 'cf83e1357eefb8bdf1542850d66d8007d620e4050b5' +
           '715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318' +
           'd2877eec2f63b931bd47417a81a538327af927da3e',
    short: '375248be5ff34be74cab4ff1c3bc8dc68bd5f8dff40' +
           '23e98f87b865cff2c724292df189443a64ff4134a65' +
           'cd4635b9d4f5dc0d3fb67528002a63acf26c9da575',
    medium: 'b9109f839e8ea43c890f293ce11dc6e2798d1e2431' +
            'f1e4b919e3b20c4f36303ba39c916db306c45a3b65' +
            '761ff5be85328eeaf42c3830f1d95e7a41165b7d2d36',
    long: '4b02caf650276030ea5617e597c5d53fd9daa68b78bfe' +
          '60b22aab8d36a4c2a3affdb71234f49276737c575ddf7' +
          '4d14054cbd6fdb98fd0ddcbcb46f91ad76b6ee'
  },
  'cshake128': {
    empty: '7f9c2ba4e88f827d616045507605853ed73b8093f6e' +
           'fbc88eb1a6eacfa66ef26',
    short: 'dea62d73e6b59cf725d0320d660089a4475cbbd3b85' +
           '39e36691f150d47556794',
    medium: 'b1acd53a03e76a221e52ea578e042f686a68c3d1c9' +
            '832ab18285cf4f304ca32d',
    long: '3a5bf5676955e5dec87d430e526925558971ca14c370' +
          'ee5d7cf572b94c7c63d7'
  },
  'cshake256': {
    empty: '46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b' +
           '81b82b50c27646ed5762fd75dc4ddd8c0f200cb0501' +
           '9d67b592f6fc821c49479ab48640292eacb3b7c4be',
    short: '1738113f5abb3ee5320ee18aa266c3617a7475dbd8e' +
           'd9a985994fddd6112ad999ec8e2ebdfeafb96e76f6b' +
           'b3a3adba43da60f00cd12496df5af3e28ae6d3de42',
    medium: '4146c13d86d9bc186b0b309ab6a124ee0c74ba26b8' +
            'c60dcc7b3ed505969aa8d19028c6317999a085b1e6' +
            'b6a785ce4ff632aeb27493227e44232fb7b3952141' +
            '7b',
    long: '0c42bfd1e282622fd8144aa29b072fd09fc2bae70885' +
          'd5290933492f9d17411926a613dd0611668c2ac999e8' +
          'c011aabaa9004323425fbad75b0f58ee6e777a94'
  },
  'sha3-256': {
    empty: 'a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a',
    short: '3059af7aa33b517084e8ad7bbc4fb208a44c28ef32b4698d103dd540e4f91aa1',
    medium: '1fa7cd1da74cd8046417508c8314e74a9a4a9d38f9f18e6cb215b8c891a0a80e',
    long: 'b2cfc61e0386cdaef5e10a2be189891f5ef52a7624bfcd8edc893acc64fec600'
  },
  'sha3-384': {
    empty: '0c63a75b845e4f7d01107d852e4c2485c51a50aaaa9' +
           '4fc61995e71bbee983a2ac3713831264adb47fb6bd1' +
           'e058d5f004',
    short: '54b8f0e4cf4974de740098f66b3024479b01631315a' +
           '6773606c33eadc32556a6e778e08f0225ae79265aec' +
           '666cb2390b',
    medium: '437b7d8b68b250b5c1739ea4cc86db2033879dfb18' +
            'de292c9c50d9c193a4c79a08a6cae3f4e483c2795e' +
            'a5d1ef7e69d2',
    long: '3b39c4c97ad87613305d0ccc987181713e2d5e84b1f9' +
          '760011bcce0c297499005bdce8a3d2409b5ad0164f32' +
          'bb8778d0'
  },
  'sha3-512': {
    empty: 'a69f73cca23a9ac5c8b567dc185a756e97c982164fe' +
           '25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9' +
           '402c3ac558f500199d95b6d3e301758586281dcd26',
    short: '2dd2e07a62e6ad0498ba84f313c4d4024cb46001f78' +
           'f75db336b0d4d8bd2a9ec152c4ad20878735d82ba08' +
           '72ecf59608ef3ced2b2a8669427e7da31e362333d8',
    medium: 'e640a21909536640369e9b0a48931c5cb2efcbc91f' +
           'ecf247306bc96a0e4ca33307cb8e1b9af367946dd01' +
           'c243f3907508d04f1692a3161df1f898de8ee25febe',
    long: 'bd262cecf565c338032de5ba0138f0aacfe7dde83d27' +
          '2d0d37d952829ed25de1a1342d98659ef7d2fa4aca7c' +
          'e2b1aa0784d8fc1dcbf81bcec7a7431a3da36bf7'
  }
};

async function testDigest(size, alg) {
  const digest = await subtle.digest(
    alg,
    Buffer.from(kSourceData[size], 'hex'));

  assert.strictEqual(
    Buffer.from(digest).toString('hex'),
    kDigestedData[(alg.name || alg).toLowerCase()][size]);
}

function applyXOF(name) {
  if (name.match(/cshake128/i)) {
    return { name, length: 256 };
  }
  if (name.match(/cshake256/i)) {
    return { name, length: 512 };
  }
  return name;

}

(async function() {
  const variations = [];
  Object.keys(kSourceData).forEach((size) => {
    Object.keys(kDigestedData).forEach((alg) => {
      const upCase = alg.toUpperCase();
      const downCase = alg.toLowerCase();
      const mixedCase = upCase.slice(0, 1) + downCase.slice(1);

      variations.push(testDigest(size, applyXOF(upCase)));
      variations.push(testDigest(size, applyXOF(downCase)));
      variations.push(testDigest(size, applyXOF(mixedCase)));
    });
  });

  await Promise.all(variations);
})().then(common.mustCall());

(async () => {
  await assert.rejects(subtle.digest('RSA-OAEP', Buffer.alloc(1)), {
    name: 'NotSupportedError',
  });
})().then(common.mustCall());

// CShake edge cases
if (getHashes().includes('shake128')) {
  (async () => {
    assert.deepStrictEqual(
      new Uint8Array(await subtle.digest({ name: 'cSHAKE128', length: 0 }, Buffer.alloc(1))),
      new Uint8Array(0),
    );

    await assert.rejects(subtle.digest({ name: 'cSHAKE128', length: 7 }, Buffer.alloc(1)), {
      name: 'NotSupportedError',
      message: 'Unsupported CShakeParams length',
    });
  })().then(common.mustCall());
}
