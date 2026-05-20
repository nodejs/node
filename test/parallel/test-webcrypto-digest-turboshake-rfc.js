'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

// RFC 9861 Section 5 test vectors

// Generates a Buffer of length n by repeating the pattern 00 01 02 .. F9 FA.
function ptn(n) {
  const buf = Buffer.allocUnsafe(n);
  for (let i = 0; i < n; i++)
    buf[i] = i % 251;
  return buf;
}

assert.deepStrictEqual(
  ptn(17 ** 2).toString('hex'),
  '000102030405060708090a0b0c0d0e0f' +
  '101112131415161718191a1b1c1d1e1f' +
  '202122232425262728292a2b2c2d2e2f' +
  '303132333435363738393a3b3c3d3e3f' +
  '404142434445464748494a4b4c4d4e4f' +
  '505152535455565758595a5b5c5d5e5f' +
  '606162636465666768696a6b6c6d6e6f' +
  '707172737475767778797a7b7c7d7e7f' +
  '808182838485868788898a8b8c8d8e8f' +
  '909192939495969798999a9b9c9d9e9f' +
  'a0a1a2a3a4a5a6a7a8a9aaabacadaeaf' +
  'b0b1b2b3b4b5b6b7b8b9babbbcbdbebf' +
  'c0c1c2c3c4c5c6c7c8c9cacbcccdcecf' +
  'd0d1d2d3d4d5d6d7d8d9dadbdcdddedf' +
  'e0e1e2e3e4e5e6e7e8e9eaebecedeeef' +
  'f0f1f2f3f4f5f6f7f8f9fa0001020304' +
  '05060708090a0b0c0d0e0f1011121314' +
  '15161718191a1b1c1d1e1f2021222324' +
  '25',
);

const turboSHAKE128Vectors = [
  // [input, outputLengthBytes, expected(, domainSeparation)]
  [new Uint8Array(0), 32,
   '1e415f1c5983aff2169217277d17bb53' +
    '8cd945a397ddec541f1ce41af2c1b74c'],
  [new Uint8Array(0), 64,
   '1e415f1c5983aff2169217277d17bb53' +
    '8cd945a397ddec541f1ce41af2c1b74c' +
    '3e8ccae2a4dae56c84a04c2385c03c15' +
    'e8193bdf58737363321691c05462c8df'],
  [ptn(17 ** 0), 32,
   '55cedd6f60af7bb29a4042ae832ef3f5' +
    '8db7299f893ebb9247247d856958daa9'],
  [ptn(17 ** 1), 32,
   '9c97d036a3bac819db70ede0ca554ec6' +
    'e4c2a1a4ffbfd9ec269ca6a111161233'],
  [ptn(17 ** 2), 32,
   '96c77c279e0126f7fc07c9b07f5cdae1' +
    'e0be60bdbe10620040e75d7223a624d2'],
  [ptn(17 ** 3), 32,
   'd4976eb56bcf118520582b709f73e1d6' +
    '853e001fdaf80e1b13e0d0599d5fb372'],
  [ptn(17 ** 4), 32,
   'da67c7039e98bf530cf7a37830c6664e' +
    '14cbab7f540f58403b1b82951318ee5c'],
  [ptn(17 ** 5), 32,
   'b97a906fbf83ef7c812517abf3b2d0ae' +
    'a0c4f60318ce11cf103925127f59eecd'],
  [ptn(17 ** 6), 32,
   '35cd494adeded2f25239af09a7b8ef0c' +
    '4d1ca4fe2d1ac370fa63216fe7b4c2b1'],
  [Buffer.from('ffffff', 'hex'), 32,
   'bf323f940494e88ee1c540fe660be8a0' +
    'c93f43d15ec006998462fa994eed5dab', 0x01],
  [Buffer.from('ff', 'hex'), 32,
   '8ec9c66465ed0d4a6c35d13506718d68' +
    '7a25cb05c74cca1e42501abd83874a67', 0x06],
  [Buffer.from('ffffff', 'hex'), 32,
   'b658576001cad9b1e5f399a9f77723bb' +
    'a05458042d68206f7252682dba3663ed', 0x07],
  [Buffer.from('ffffffffffffff', 'hex'), 32,
   '8deeaa1aec47ccee569f659c21dfa8e1' +
    '12db3cee37b18178b2acd805b799cc37', 0x0b],
  [Buffer.from('ff', 'hex'), 32,
   '553122e2135e363c3292bed2c6421fa2' +
    '32bab03daa07c7d6636603286506325b', 0x30],
  [Buffer.from('ffffff', 'hex'), 32,
   '16274cc656d44cefd422395d0f9053bd' +
    'a6d28e122aba15c765e5ad0e6eaf26f9', 0x7f],
];

const turboSHAKE256Vectors = [
  // [input, outputLengthBytes, expected(, domainSeparation)]
  [new Uint8Array(0), 64,
   '367a329dafea871c7802ec67f905ae13' +
    'c57695dc2c6663c61035f59a18f8e7db' +
    '11edc0e12e91ea60eb6b32df06dd7f00' +
    '2fbafabb6e13ec1cc20d995547600db0'],
  [ptn(17 ** 0), 64,
   '3e1712f928f8eaf1054632b2aa0a246e' +
    'd8b0c378728f60bc970410155c28820e' +
    '90cc90d8a3006aa2372c5c5ea176b068' +
    '2bf22bae7467ac94f74d43d39b0482e2'],
  [ptn(17 ** 1), 64,
   'b3bab0300e6a191fbe61379398359235' +
    '78794ea54843f5011090fa2f3780a9e5' +
    'cb22c59d78b40a0fbff9e672c0fbe097' +
    '0bd2c845091c6044d687054da5d8e9c7'],
  [ptn(17 ** 2), 64,
   '66b810db8e90780424c0847372fdc957' +
    '10882fde31c6df75beb9d4cd9305cfca' +
    'e35e7b83e8b7e6eb4b78605880116316' +
    'fe2c078a09b94ad7b8213c0a738b65c0'],
  [ptn(17 ** 3), 64,
   'c74ebc919a5b3b0dd1228185ba02d29e' +
    'f442d69d3d4276a93efe0bf9a16a7dc0' +
    'cd4eabadab8cd7a5edd96695f5d360ab' +
    'e09e2c6511a3ec397da3b76b9e1674fb'],
  [ptn(17 ** 4), 64,
   '02cc3a8897e6f4f6ccb6fd46631b1f52' +
    '07b66c6de9c7b55b2d1a23134a170afd' +
    'ac234eaba9a77cff88c1f020b7372461' +
    '8c5687b362c430b248cd38647f848a1d'],
  [ptn(17 ** 5), 64,
   'add53b06543e584b5823f626996aee50' +
    'fe45ed15f20243a7165485acb4aa76b4' +
    'ffda75cedf6d8cdc95c332bd56f4b986' +
    'b58bb17d1778bfc1b1a97545cdf4ec9f'],
  [ptn(17 ** 6), 64,
   '9e11bc59c24e73993c1484ec66358ef7' +
    '1db74aefd84e123f7800ba9c4853e02c' +
    'fe701d9e6bb765a304f0dc34a4ee3ba8' +
    '2c410f0da70e86bfbd90ea877c2d6104'],
  [Buffer.from('ffffff', 'hex'), 64,
   'd21c6fbbf587fa2282f29aea620175fb' +
    '0257413af78a0b1b2a87419ce031d933' +
    'ae7a4d383327a8a17641a34f8a1d1003' +
    'ad7da6b72dba84bb62fef28f62f12424', 0x01],
  [Buffer.from('ff', 'hex'), 64,
   '738d7b4e37d18b7f22ad1b5313e357e3' +
    'dd7d07056a26a303c433fa3533455280' +
    'f4f5a7d4f700efb437fe6d281405e07b' +
    'e32a0a972e22e63adc1b090daefe004b', 0x06],
  [Buffer.from('ffffff', 'hex'), 64,
   '18b3b5b7061c2e67c1753a00e6ad7ed7' +
    'ba1c906cf93efb7092eaf27fbeebb755' +
    'ae6e292493c110e48d260028492b8e09' +
    'b5500612b8f2578985ded5357d00ec67', 0x07],
  [Buffer.from('ffffffffffffff', 'hex'), 64,
   'bb36764951ec97e9d85f7ee9a67a7718' +
    'fc005cf42556be79ce12c0bde50e5736' +
    'd6632b0d0dfb202d1bbb8ffe3dd74cb0' +
    '0834fa756cb03471bab13a1e2c16b3c0', 0x0b],
  [Buffer.from('ff', 'hex'), 64,
   'f3fe12873d34bcbb2e608779d6b70e7f' +
    '86bec7e90bf113cbd4fdd0c4e2f4625e' +
    '148dd7ee1a52776cf77f240514d9ccfc' +
    '3b5ddab8ee255e39ee389072962c111a', 0x30],
  [Buffer.from('ffffff', 'hex'), 64,
   'abe569c1f77ec340f02705e7d37c9ab7' +
    'e155516e4a6a150021d70b6fac0bb40c' +
    '069f9a9828a0d575cd99f9bae435ab1a' +
    'cf7ed9110ba97ce0388d074bac768776', 0x7f],
];

const kt128Vectors = [
  // [input, outputLengthBytes, expected(, customization)]
  [new Uint8Array(0), 32,
   '1ac2d450fc3b4205d19da7bfca1b3751' +
    '3c0803577ac7167f06fe2ce1f0ef39e5'],
  [new Uint8Array(0), 64,
   '1ac2d450fc3b4205d19da7bfca1b3751' +
    '3c0803577ac7167f06fe2ce1f0ef39e5' +
    '4269c056b8c82e48276038b6d292966c' +
    'c07a3d4645272e31ff38508139eb0a71'],
  [ptn(1), 32,
   '2bda92450e8b147f8a7cb629e784a058' +
    'efca7cf7d8218e02d345dfaa65244a1f'],
  [ptn(17), 32,
   '6bf75fa2239198db4772e36478f8e19b' +
    '0f371205f6a9a93a273f51df37122888'],
  [ptn(17 ** 2), 32,
   '0c315ebcdedbf61426de7dcf8fb725d1' +
    'e74675d7f5327a5067f367b108ecb67c'],
  [ptn(17 ** 3), 32,
   'cb552e2ec77d9910701d578b457ddf77' +
    '2c12e322e4ee7fe417f92c758f0d59d0'],
  [ptn(17 ** 4), 32,
   '8701045e22205345ff4dda05555cbb5c' +
    '3af1a771c2b89baef37db43d9998b9fe'],
  [ptn(17 ** 5), 32,
   '844d610933b1b9963cbdeb5ae3b6b05c' +
    'c7cbd67ceedf883eb678a0a8e0371682'],
  [ptn(17 ** 6), 32,
   '3c390782a8a4e89fa6367f72feaaf132' +
    '55c8d95878481d3cd8ce85f58e880af8'],
  [new Uint8Array(0), 32,
   'fab658db63e94a246188bf7af69a1330' +
    '45f46ee984c56e3c3328caaf1aa1a583', ptn(1)],
  [Buffer.from('ff', 'hex'), 32,
   'd848c5068ced736f4462159b9867fd4c' +
    '20b808acc3d5bc48e0b06ba0a3762ec4', ptn(41)],
  [Buffer.from('ffffff', 'hex'), 32,
   'c389e5009ae57120854c2e8c64670ac0' +
    '1358cf4c1baf89447a724234dc7ced74', ptn(41 ** 2)],
  [Buffer.from('ffffffffffffff', 'hex'), 32,
   '75d2f86a2e644566726b4fbcfc5657b9' +
    'dbcf070c7b0dca06450ab291d7443bcf', ptn(41 ** 3)],
  [ptn(8191), 32,
   '1b577636f723643e990cc7d6a6598374' +
    '36fd6a103626600eb8301cd1dbe553d6'],
  [ptn(8192), 32,
   '48f256f6772f9edfb6a8b661ec92dc93' +
    'b95ebd05a08a17b39ae3490870c926c3'],
  [ptn(8192), 32,
   '3ed12f70fb05ddb58689510ab3e4d23c' +
    '6c6033849aa01e1d8c220a297fedcd0b', ptn(8189)],
  [ptn(8192), 32,
   '6a7c1b6a5cd0d8c9ca943a4a216cc646' +
    '04559a2ea45f78570a15253d67ba00ae', ptn(8190)],
];

const kt256Vectors = [
  // [input, outputLengthBytes, expected(, customization)]
  [new Uint8Array(0), 64,
   'b23d2e9cea9f4904e02bec06817fc10c' +
    'e38ce8e93ef4c89e6537076af8646404' +
    'e3e8b68107b8833a5d30490aa3348235' +
    '3fd4adc7148ecb782855003aaebde4a9'],
  [new Uint8Array(0), 128,
   'b23d2e9cea9f4904e02bec06817fc10c' +
    'e38ce8e93ef4c89e6537076af8646404' +
    'e3e8b68107b8833a5d30490aa3348235' +
    '3fd4adc7148ecb782855003aaebde4a9' +
    'b0925319d8ea1e121a609821ec19efea' +
    '89e6d08daee1662b69c840289f188ba8' +
    '60f55760b61f82114c030c97e5178449' +
    '608ccd2cd2d919fc7829ff69931ac4d0'],
  [ptn(1), 64,
   '0d005a194085360217128cf17f91e1f7' +
    '1314efa5564539d444912e3437efa17f' +
    '82db6f6ffe76e781eaa068bce01f2bbf' +
    '81eacb983d7230f2fb02834a21b1ddd0'],
  [ptn(17), 64,
   '1ba3c02b1fc514474f06c8979978a905' +
    '6c8483f4a1b63d0dccefe3a28a2f323e' +
    '1cdcca40ebf006ac76ef039715234683' +
    '7b1277d3e7faa9c9653b19075098527b'],
  [ptn(17 ** 2), 64,
   'de8ccbc63e0f133ebb4416814d4c66f6' +
    '91bbf8b6a61ec0a7700f836b086cb029' +
    'd54f12ac7159472c72db118c35b4e6aa' +
    '213c6562caaa9dcc518959e69b10f3ba'],
  [ptn(17 ** 3), 64,
   '647efb49fe9d717500171b41e7f11bd4' +
    '91544443209997ce1c2530d15eb1ffbb' +
    '598935ef954528ffc152b1e4d731ee26' +
    '83680674365cd191d562bae753b84aa5'],
  [ptn(17 ** 4), 64,
   'b06275d284cd1cf205bcbe57dccd3ec1' +
    'ff6686e3ed15776383e1f2fa3c6ac8f0' +
    '8bf8a162829db1a44b2a43ff83dd89c3' +
    'cf1ceb61ede659766d5ccf817a62ba8d'],
  [ptn(17 ** 5), 64,
   '9473831d76a4c7bf77ace45b59f1458b' +
    '1673d64bcd877a7c66b2664aa6dd149e' +
    '60eab71b5c2bab858c074ded81ddce2b' +
    '4022b5215935c0d4d19bf511aeeb0772'],
  [ptn(17 ** 6), 64,
   '0652b740d78c5e1f7c8dcc1777097382' +
    '768b7ff38f9a7a20f29f413bb1b3045b' +
    '31a5578f568f911e09cf44746da84224' +
    'a5266e96a4a535e871324e4f9c7004da'],
  [new Uint8Array(0), 64,
   '9280f5cc39b54a5a594ec63de0bb9937' +
    '1e4609d44bf845c2f5b8c316d72b1598' +
    '11f748f23e3fabbe5c3226ec96c62186' +
    'df2d33e9df74c5069ceecbb4dd10eff6', ptn(1)],
  [Buffer.from('ff', 'hex'), 64,
   '47ef96dd616f200937aa7847e34ec2fe' +
    'ae8087e3761dc0f8c1a154f51dc9ccf8' +
    '45d7adbce57ff64b639722c6a1672e3b' +
    'f5372d87e00aff89be97240756998853', ptn(41)],
  [Buffer.from('ffffff', 'hex'), 64,
   '3b48667a5051c5966c53c5d42b95de45' +
    '1e05584e7806e2fb765eda959074172c' +
    'b438a9e91dde337c98e9c41bed94c4e0' +
    'aef431d0b64ef2324f7932caa6f54969', ptn(41 ** 2)],
  [Buffer.from('ffffffffffffff', 'hex'), 64,
   'e0911cc00025e1540831e266d94add9b' +
    '98712142b80d2629e643aac4efaf5a3a' +
    '30a88cbf4ac2a91a2432743054fbcc98' +
    '97670e86ba8cec2fc2ace9c966369724', ptn(41 ** 3)],
  [ptn(8191), 64,
   '3081434d93a4108d8d8a3305b89682ce' +
    'bedc7ca4ea8a3ce869fbb73cbe4a58ee' +
    'f6f24de38ffc170514c70e7ab2d01f03' +
    '812616e863d769afb3753193ba045b20'],
  [ptn(8192), 64,
   'c6ee8e2ad3200c018ac87aaa031cdac2' +
    '2121b412d07dc6e0dccbb53423747e9a' +
    '1c18834d99df596cf0cf4b8dfafb7bf0' +
    '2d139d0c9035725adc1a01b7230a41fa'],
  [ptn(8192), 64,
   '74e47879f10a9c5d11bd2da7e194fe57' +
    'e86378bf3c3f7448eff3c576a0f18c5c' +
    'aae0999979512090a7f348af4260d4de' +
    '3c37f1ecaf8d2c2c96c1d16c64b12496', ptn(8189)],
  [ptn(8192), 64,
   'f4b5908b929ffe01e0f79ec2f21243d4' +
    '1a396b2e7303a6af1d6399cd6c7a0a2d' +
    'd7c4f607e8277f9c9b1cb4ab9ddc59d4' +
    'b92d1fc7558441f1832c3279a4241b8b', ptn(8190)],
];

async function checkDigest(name, vectors) {
  const isKT = name.startsWith('KT');
  for (const [input, outputLength, expected, ...rest] of vectors) {
    const algorithm = { name, outputLength: outputLength * 8 };
    if (rest.length) {
      if (isKT)
        algorithm.customization = rest[0];
      else
        algorithm.domainSeparation = rest[0];
    }
    const result = await subtle.digest(algorithm, input);
    assert.deepStrictEqual(
      Buffer.from(result).toString('hex'),
      expected,
    );
  }
}

(async () => {
  await checkDigest('TurboSHAKE128', turboSHAKE128Vectors);

  // TurboSHAKE128(M=00^0, D=1F, 10032), last 32 bytes
  {
    const result = await subtle.digest({
      name: 'TurboSHAKE128',
      outputLength: 10032 * 8,
    }, new Uint8Array(0));
    assert.deepStrictEqual(
      Buffer.from(result).subarray(-32).toString('hex'),
      'a3b9b0385900ce761f22aed548e754da' +
      '10a5242d62e8c658e3f3a923a7555607',
    );
  }

  await checkDigest('TurboSHAKE256', turboSHAKE256Vectors);

  // TurboSHAKE256(M=00^0, D=1F, 10032), last 32 bytes
  {
    const result = await subtle.digest({
      name: 'TurboSHAKE256',
      outputLength: 10032 * 8,
    }, new Uint8Array(0));
    assert.deepStrictEqual(
      Buffer.from(result).subarray(-32).toString('hex'),
      'abefa11630c661269249742685ec082f' +
      '207265dccf2f43534e9c61ba0c9d1d75',
    );
  }

  await checkDigest('KT128', kt128Vectors);

  // KT128(M=00^0, C=00^0, 10032), last 32 bytes
  {
    const result = await subtle.digest({
      name: 'KT128',
      outputLength: 10032 * 8,
    }, new Uint8Array(0));
    assert.deepStrictEqual(
      Buffer.from(result).subarray(-32).toString('hex'),
      'e8dc563642f7228c84684c898405d3a8' +
      '34799158c079b12880277a1d28e2ff6d',
    );
  }

  await checkDigest('KT256', kt256Vectors);

  // KT256(M=00^0, C=00^0, 10064), last 64 bytes
  {
    const result = await subtle.digest({
      name: 'KT256',
      outputLength: 10064 * 8,
    }, new Uint8Array(0));
    assert.deepStrictEqual(
      Buffer.from(result).subarray(-64).toString('hex'),
      'ad4a1d718cf950506709a4c33396139b' +
      '4449041fc79a05d68da35f1e453522e0' +
      '56c64fe94958e7085f2964888259b993' +
      '2752f3ccd855288efee5fcbb8b563069',
    );
  }
})().then(common.mustCall());
