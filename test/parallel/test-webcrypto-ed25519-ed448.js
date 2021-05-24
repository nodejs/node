'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPairSync,
  webcrypto: { subtle }
} = require('crypto');

async function generateKey(namedCurve) {
  return subtle.generateKey(
    {
      name: namedCurve,
      namedCurve
    },
    true,
    ['sign', 'verify']);
}

async function test1(namedCurve) {
  const {
    publicKey,
    privateKey,
  } = await generateKey(namedCurve);

  const data = Buffer.from('hello world');

  assert(publicKey);
  assert(privateKey);

  const sig = await subtle.sign(
    { name: namedCurve },
    privateKey,
    data
  );

  assert(sig);

  assert(await subtle.verify(
    { name: namedCurve },
    publicKey,
    sig,
    data
  ));
}

Promise.all([
  test1('NODE-ED25519'),
  test1('NODE-ED448'),
]).then(common.mustCall());

assert.rejects(
  subtle.importKey(
    'raw',
    Buffer.alloc(10),
    {
      name: 'NODE-ED25519',
      namedCurve: 'NODE-ED25519'
    },
    false,
    ['sign']),
  {
    message: /NODE-ED25519 raw keys must be exactly 32-bytes/
  }).then(common.mustCall());

assert.rejects(
  subtle.importKey(
    'raw',
    Buffer.alloc(10),
    {
      name: 'NODE-ED448',
      namedCurve: 'NODE-ED448'
    },
    false,
    ['sign']),
  {
    message: /NODE-ED448 raw keys must be exactly 57-bytes/
  }).then(common.mustCall());

const testVectors = {
  'NODE-ED25519': [
    {
      privateKey:
        Buffer.from(
          '9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60',
          'hex'),
      publicKey:
        Buffer.from(
          'd75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a',
          'hex'),
      message: Buffer.alloc(0),
      sig:
        Buffer.from(
          'e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155' +
          '5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b',
          'hex'),
      crv: 'Ed25519',
    },
    {
      privateKey:
        Buffer.from(
          '4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb',
          'hex'),
      publicKey:
        Buffer.from(
          '3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c',
          'hex'),
      message: Buffer.from('72', 'hex'),
      sig:
        Buffer.from(
          '92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da' +
          '085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00',
          'hex'),
      crv: 'Ed25519',
    },
    {
      privateKey:
        Buffer.from(
          'c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7',
          'hex'),
      publicKey:
        Buffer.from(
          'fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025',
          'hex'),
      message: Buffer.from('af82', 'hex'),
      sig:
        Buffer.from(
          '6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac' +
          '18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a',
          'hex'),
      crv: 'Ed25519',
    },
    {
      privateKey:
        Buffer.from(
          'f5e5767cf153319517630f226876b86c8160cc583bc013744c6bf255f5cc0ee5',
          'hex'),
      publicKey:
        Buffer.from(
          '278117fc144c72340f67d0f2316e8386ceffbf2b2428c9c51fef7c597f1d426e',
          'hex'),
      message: Buffer.from(
        '08b8b2b733424243760fe426a4b54908632110a66c2f6591eabd3345e3e4eb98' +
        'fa6e264bf09efe12ee50f8f54e9f77b1e355f6c50544e23fb1433ddf73be84d8' +
        '79de7c0046dc4996d9e773f4bc9efe5738829adb26c81b37c93a1b270b20329d' +
        '658675fc6ea534e0810a4432826bf58c941efb65d57a338bbd2e26640f89ffbc' +
        '1a858efcb8550ee3a5e1998bd177e93a7363c344fe6b199ee5d02e82d522c4fe' +
        'ba15452f80288a821a579116ec6dad2b3b310da903401aa62100ab5d1a36553e' +
        '06203b33890cc9b832f79ef80560ccb9a39ce767967ed628c6ad573cb116dbef' +
        'efd75499da96bd68a8a97b928a8bbc103b6621fcde2beca1231d206be6cd9ec7' +
        'aff6f6c94fcd7204ed3455c68c83f4a41da4af2b74ef5c53f1d8ac70bdcb7ed1' +
        '85ce81bd84359d44254d95629e9855a94a7c1958d1f8ada5d0532ed8a5aa3fb2' +
        'd17ba70eb6248e594e1a2297acbbb39d502f1a8c6eb6f1ce22b3de1a1f40cc24' +
        '554119a831a9aad6079cad88425de6bde1a9187ebb6092cf67bf2b13fd65f270' +
        '88d78b7e883c8759d2c4f5c65adb7553878ad575f9fad878e80a0c9ba63bcbcc' +
        '2732e69485bbc9c90bfbd62481d9089beccf80cfe2df16a2cf65bd92dd597b07' +
        '07e0917af48bbb75fed413d238f5555a7a569d80c3414a8d0859dc65a46128ba' +
        'b27af87a71314f318c782b23ebfe808b82b0ce26401d2e22f04d83d1255dc51a' +
        'ddd3b75a2b1ae0784504df543af8969be3ea7082ff7fc9888c144da2af58429e' +
        'c96031dbcad3dad9af0dcbaaaf268cb8fcffead94f3c7ca495e056a9b47acdb7' +
        '51fb73e666c6c655ade8297297d07ad1ba5e43f1bca32301651339e22904cc8c' +
        '42f58c30c04aafdb038dda0847dd988dcda6f3bfd15c4b4c4525004aa06eeff8' +
        'ca61783aacec57fb3d1f92b0fe2fd1a85f6724517b65e614ad6808d6f6ee34df' +
        'f7310fdc82aebfd904b01e1dc54b2927094b2db68d6f903b68401adebf5a7e08' +
        'd78ff4ef5d63653a65040cf9bfd4aca7984a74d37145986780fc0b16ac451649' +
        'de6188a7dbdf191f64b5fc5e2ab47b57f7f7276cd419c17a3ca8e1b939ae49e4' +
        '88acba6b965610b5480109c8b17b80e1b7b750dfc7598d5d5011fd2dcc5600a3' +
        '2ef5b52a1ecc820e308aa342721aac0943bf6686b64b2579376504ccc493d97e' +
        '6aed3fb0f9cd71a43dd497f01f17c0e2cb3797aa2a2f256656168e6c496afc5f' +
        'b93246f6b1116398a346f1a641f3b041e989f7914f90cc2c7fff357876e506b5' +
        '0d334ba77c225bc307ba537152f3f1610e4eafe595f6d9d90d11faa933a15ef1' +
        '369546868a7f3a45a96768d40fd9d03412c091c6315cf4fde7cb68606937380d' +
        'b2eaaa707b4c4185c32eddcdd306705e4dc1ffc872eeee475a64dfac86aba41c' +
        '0618983f8741c5ef68d3a101e8a3b8cac60c905c15fc910840b94c00a0b9d0',
        'hex'),
      sig: Buffer.from(
        '0aab4c900501b3e24d7cdf4663326a3a87df5e4843b2cbdb67cbf6e460fec350' +
        'aa5371b1508f9f4528ecea23c436d94b5e8fcd4f681e30a6ac00a9704a188a03',
        'hex'),
      crv: 'Ed25519',
    },
  ],
  'NODE-ED448': [
    {
      privateKey:
        Buffer.from(
          '6c82a562cb808d10d632be89c8513ebf6c929f34ddfa8c9f63c9960ef6e348a3' +
          '528c8a3fcc2f044e39a3fc5b94492f8f032e7549a20098f95b', 'hex'),
      publicKey:
        Buffer.from(
          '5fd7449b59b461fd2ce787ec616ad46a1da1342485a70e1f8a0ea75d80e96778' +
          'edf124769b46c7061bd6783df1e50f6cd1fa1abeafe8256180', 'hex'),
      message: Buffer.alloc(0),
      sig:
        Buffer.from(
          '533a37f6bbe457251f023c0d88f976ae2dfb504a843e34d2074fd823d41a591f' +
          '2b233f034f628281f2fd7a22ddd47d7828c59bd0a21bfd3980ff0d2028d4b18a' +
          '9df63e006c5d1c2d345b925d8dc00b4104852db99ac5c7cdda8530a113a0f4db' +
          'b61149f05a7363268c71d95808ff2e652600', 'hex'),
      crv: 'Ed448',
    },
    {
      privateKey:
        Buffer.from(
          'c4eab05d357007c632f3dbb48489924d552b08fe0c353a0d4a1f00acda2c463a' +
          'fbea67c5e8d2877c5e3bc397a659949ef8021e954e0a12274e', 'hex'),
      publicKey:
        Buffer.from(
          '43ba28f430cdff456ae531545f7ecd0ac834a55d9358c0372bfa0c6c6798c086' +
          '6aea01eb00742802b8438ea4cb82169c235160627b4c3a9480', 'hex'),
      message: Buffer.from('03', 'hex'),
      sig:
        Buffer.from(
          '26b8f91727bd62897af15e41eb43c377efb9c610d48f2335cb0bd0087810f435' +
          '2541b143c4b981b7e18f62de8ccdf633fc1bf037ab7cd779805e0dbcc0aae1cb' +
          'cee1afb2e027df36bc04dcecbf154336c19f0af7e0a6472905e799f1953d2a0f' +
          'f3348ab21aa4adafd1d234441cf807c03a00', 'hex'),
      crv: 'Ed448',
    },
    {
      privateKey:
        Buffer.from(
          'cd23d24f714274e744343237b93290f511f6425f98e64459ff203e8985083ffd' +
          'f60500553abc0e05cd02184bdb89c4ccd67e187951267eb328', 'hex'),
      publicKey:
        Buffer.from(
          'dcea9e78f35a1bf3499a831b10b86c90aac01cd84b67a0109b55a36e9328b1e3' +
          '65fce161d71ce7131a543ea4cb5f7e9f1d8b00696447001400', 'hex'),
      message: Buffer.from('0c3e544074ec63b0265e0c', 'hex'),
      sig:
        Buffer.from(
          '1f0a8888ce25e8d458a21130879b840a9089d999aaba039eaf3e3afa090a09d3' +
          '89dba82c4ff2ae8ac5cdfb7c55e94d5d961a29fe0109941e00b8dbdeea6d3b05' +
          '1068df7254c0cdc129cbe62db2dc957dbb47b51fd3f213fb8698f064774250a5' +
          '028961c9bf8ffd973fe5d5c206492b140e00', 'hex'),
      crv: 'Ed448',
    },
  ]
};

async function test2(namedCurve) {
  const vectors = testVectors[namedCurve];
  await Promise.all(vectors.map(async (vector) => {
    const [
      privateKey,
      publicKey,
    ] = await Promise.all([
      subtle.importKey(
        'raw',
        vector.privateKey,
        {
          name: namedCurve,
          namedCurve
        },
        true, ['sign']),
      subtle.importKey(
        'raw',
        vector.publicKey,
        {
          name: namedCurve,
          namedCurve,
          public: true
        },
        true, ['verify']),
    ]);

    const rawPublicKey = await subtle.exportKey('raw', publicKey);
    assert.deepStrictEqual(Buffer.from(rawPublicKey), vector.publicKey);

    assert.rejects(subtle.exportKey('raw', privateKey), {
      message: new RegExp(`Unable to export a raw ${namedCurve} private key`)
    }).then(common.mustCall());

    const sig = await subtle.sign(
      { name: namedCurve },
      privateKey,
      vector.message
    );

    assert(sig);

    assert(await subtle.verify(
      { name: namedCurve },
      publicKey,
      vector.sig,
      vector.message
    ));

    const [
      publicKeyJwk,
      privateKeyJwk,
    ] = await Promise.all([
      subtle.exportKey('jwk', publicKey),
      subtle.exportKey('jwk', privateKey),
    ]);
    assert.strictEqual(publicKeyJwk.kty, 'OKP');
    assert.strictEqual(privateKeyJwk.kty, 'OKP');
    assert.strictEqual(publicKeyJwk.crv, vector.crv);
    assert.strictEqual(privateKeyJwk.crv, vector.crv);
    assert.deepStrictEqual(
      Buffer.from(publicKeyJwk.x, 'base64'),
      vector.publicKey);
    assert.deepStrictEqual(
      Buffer.from(privateKeyJwk.x, 'base64'),
      vector.publicKey);
    assert.deepStrictEqual(
      Buffer.from(privateKeyJwk.d, 'base64'),
      vector.privateKey);
  }));
}

Promise.all([
  test2('NODE-ED25519'),
  test2('NODE-ED448'),
]).then(common.mustCall());

assert.rejects(
  subtle.generateKey(
    {
      name: 'ECDSA',
      namedCurve: 'NODE-X25519'
    },
    true,
    ['sign', 'verify']),
  {
    message: /Unsupported named curves for ECDSA/
  }).then(common.mustCall());

assert.rejects(
  subtle.generateKey(
    {
      name: 'ECDSA',
      namedCurve: 'NODE-X448'
    },
    true,
    ['sign', 'verify']),
  {
    message: /Unsupported named curves for ECDSA/
  }).then(common.mustCall());

assert.rejects(
  subtle.generateKey(
    {
      name: 'ECDSA',
      namedCurve: 'NODE-ED25519'
    },
    true,
    ['sign', 'verify']),
  {
    message: /Unsupported named curves for ECDSA/
  }).then(common.mustCall());

assert.rejects(
  subtle.generateKey(
    {
      name: 'ECDSA',
      namedCurve: 'NODE-ED448'
    },
    true,
    ['sign', 'verify']),
  {
    message: /Unsupported named curves for ECDSA/
  }).then(common.mustCall());

{
  for (const asymmetricKeyType of ['ed25519', 'ed448']) {
    const { publicKey, privateKey } = generateKeyPairSync(asymmetricKeyType);
    for (const keyObject of [publicKey, privateKey]) {
      const namedCurve = `NODE-${asymmetricKeyType.toUpperCase()}`;
      subtle.importKey(
        'node.keyObject',
        keyObject,
        { name: namedCurve, namedCurve },
        true,
        keyObject.type === 'private' ? ['sign'] : ['verify'],
      ).then((cryptoKey) => {
        assert.strictEqual(cryptoKey.type, keyObject.type);
        assert.strictEqual(cryptoKey.algorithm.name, namedCurve);
      }, common.mustNotCall());

      assert.rejects(
        subtle.importKey(
          'node.keyObject',
          keyObject,
          {
            name: 'ECDSA',
            namedCurve,
          },
          true,
          keyObject.type === 'private' ? ['sign'] : ['verify']
        ),
        {
          message: /Invalid algorithm name/
        }).then(common.mustCall());

      assert.rejects(
        subtle.importKey(
          'node.keyObject',
          keyObject,
          {
            name: 'ECDH',
            namedCurve,
          },
          true,
          keyObject.type === 'private' ? ['deriveBits', 'deriveKey'] : [],
        ),
        {
          message: /Invalid algorithm name/
        }).then(common.mustCall());
    }
  }
}
