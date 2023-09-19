'use strict';

module.exports = function() {
  const pkcs8 = Buffer.from(
    '308204bf020100300d06092a864886f70d0101010500048204a930820' +
    '4a50201000282010100d3576092e62957364544e7e4233b7bdb293db2' +
    '085122c479328546f9f0f712f657c4b17868c930908cc594f7ed00c01' +
    '442c1af04c2f678a48ba2c80fd1713e30b5ac50787ac3516589f17196' +
    '7f6386ada34900a6bb04eecea42bf043ced9a0f94d0cc09e919b9d716' +
    '6c08ab6ce204640aea4c4920db6d86eb916d0dcc0f4341a10380429e7' +
    'e1032144ea949de8f6c0ccbf95fa8e928d70d8a38ce168db45f6f1346' +
    '63d6f656f5ceabc725da8c02aabeaaa13ac36a75cc0bae135df3114b6' +
    '6589c7ed3cb61559ae5a384f162bfa80dbe4617f86c3f1d010c94fe2c' +
    '9bf019a6e63b3efc028d43cee611c85ec263c906c463772c6911b19ee' +
    'c096ca76ec5e31e1e3020301000102820101008b375ccb87c825c5ff3' +
    'd53d009916e9641057e18527227a07ab226be1088813a3b38bb7b48f3' +
    '77055165fa2a9339d24dc667d5c5ba3427e6a481176eac15ffd490683' +
    '11e1c283b9f3a8e0cb809b4630c50aa8f3e45a60b359e19bf8cbb5eca' +
    'd64e761f1095743ff36aaf5cf0ecb97fedaddda60b5bf35d811a75b82' +
    '2230cfaa0192fad40547e275448aa3316bf8e2b4ce0854fc7708b537b' +
    'a22d13210b09aec37a2759efc082a1531b23a91730037dde4ef26b5f9' +
    '6efdcc39fd34c345ad51cbbe44fe58b8a3b4ec997866c086dff1b8831' +
    'ef0a1fea263cf7dacd03c04cbcc2b279e57fa5b953996bfb1dd68817a' +
    'f7fb42cdef7a5294a57fac2b8ad739f1b029902818100fbf833c2c631' +
    'c970240c8e7485f06a3ea2a84822511a8627dd464ef8afaf7148d1a42' +
    '5b6b8657ddd5246832b8e533020c5bbb568855a6aec3e4221d793f1dc' +
    '5b2f2584e2415e48e9a2bd292b134031f99c8eb42fc0bcd0449bf22ce' +
    '6dec97014efe5ac93ebe835877656252cbbb16c415b67b184d2284568' +
    'a277d59335585cfd02818100d6b8ce27c7295d5d16fc3570ed64c8da9' +
    '303fad29488c1a65e9ad711f90370187dbbfd81316d69648bc88cc5c8' +
    '3551afff45debacfb61105f709e4c30809b90031ebd686244496c6f69' +
    'e692ebdc814f64239f4ad15756ecb78c5a5b09931db183077c546a38c' +
    '4c743889ad3d3ed079b5622ed0120fa0e1f93b593db7d852e05f02818' +
    '038874b9d83f78178ce2d9efc175c83897fd67f306bbfa69f64ee3423' +
    '68ced47c80c3f1ce177a758d64bafb0c9786a44285fa01cdec3507cde' +
    'e7dc9b7e2b21d3cbbcc100eee9967843b057329fdcca62998ed0f11b3' +
    '8ce8b0abc7de39017c71cfd0ae57546c559144cdd0afd0645f7ea8ff0' +
    '7b974d1ed44fd1f8e00f560bf6d45028181008529ef9073cf8f7b5ff9' +
    'e21abadf3a4173d3900670dfaf59426abcdf0493c13d2f1d1b46b824a' +
    '6ac1894b3d925250c181e3472c16078056eb19a8d28f71f3080927534' +
    '81d49444fdf78c9ea6c24407dc018e77d3afef385b2ff7439e9623794' +
    '1332dd446cebeffdb4404fe4f71595161d016402c334d0f57c61abe4f' +
    'f9f4cbf90281810087d87708d46763e4ccbeb2d1e9712e5bf0216d70d' +
    'e9420a5b2069b7459b99f5d9f7f2fad7cd79aaee67a7f9a34437e3c79' +
    'a84af0cd8de9dff268eb0c4793f501f988d540f6d3475c2079b8227a2' +
    '3d968dec4e3c66503187193459630472bfdb6ba1de786c797fa6f4ea6' +
    '5a2a8419262f29678856cb73c9bd4bc89b5e041b2277', 'hex');

  const spki = Buffer.from(
    '30820122300d06092a864886f70d01010105000382010f003082010a0' +
    '282010100d3576092e62957364544e7e4233b7bdb293db2085122c479' +
    '328546f9f0f712f657c4b17868c930908cc594f7ed00c01442c1af04c' +
    '2f678a48ba2c80fd1713e30b5ac50787ac3516589f171967f6386ada3' +
    '4900a6bb04eecea42bf043ced9a0f94d0cc09e919b9d7166c08ab6ce2' +
    '04640aea4c4920db6d86eb916d0dcc0f4341a10380429e7e1032144ea' +
    '949de8f6c0ccbf95fa8e928d70d8a38ce168db45f6f134663d6f656f5' +
    'ceabc725da8c02aabeaaa13ac36a75cc0bae135df3114b66589c7ed3c' +
    'b61559ae5a384f162bfa80dbe4617f86c3f1d010c94fe2c9bf019a6e6' +
    '3b3efc028d43cee611c85ec263c906c463772c6911b19eec096ca76ec' +
    '5e31e1e30203010001', 'hex');

  const label = Buffer.from(
    '5468657265206172652037206675727468657220656469746f7269616' +
    'c206e6f74657320696e2074686520646f63756d656e742e', 'hex');

    // overlong plaintext for RSA-OAEP
  const plaintext = Buffer.from(
    '5f4dba4f320c0ce876725afce5fbd25bf83e5a7125a08cafe73c3ebac4' +
    '21779df9d55d180c3ae9942645e1d82fee8c9d294b3cb1a08a9931201b' +
    '3c0e81fc47cacf8315a2af66324113c3b66230c34608c4f4593634ce02' +
    'b267362277f0a840ca74bc3d1a6236952c5ed7aaf8a8fecbddfa7584e6' +
    '978cea5d2a5b9fb7f1b48c8b0be58a305202754d8376107374793cf026' +
    'aaee5300727d836cd71e71b345ddb2e44446ffc5b901635413890d910e' +
    'a380984a90191031323f16dbcc9d6be168b84885384ca03e12600ac1c2' +
    '48028af3726cc93463882ea8c02aab', 'hex');

  const ciphertext = {
    'sha-1, no label': Buffer.from(
      '901ef09cbbfe9a4939b9a9c43ccf22339e1575f7c74324494075c192' +
      'c40b68996eb0e04d7b151774fff7c00b66174a249fc3d1a6123c70a6' +
      '6fd61b67cb54f683fe01049e4a44a5937e35cae1b73dce12aea09cd0' +
      '1c4c48900fafdd753ac401566076b9b863807cf141a63044865e0cbc' +
      '42b995fb639fe9994e94c0e381404ae1b554cdb27515f09798dc5a07' +
      'a60b162bf12f8cef7ce166314d942d80fe43d0df1fb0d7e9514ef729' +
      'dc6c845583f74ab2c36d9684d43b71962a18ff0e2b13ce74f537fb3a' +
      '0b00ede329e77c11900a070e20f86dc07cacb56f7821d0249234106c' +
      '6e0b4dda82e0febdb202ef0c7b10d560f0bafdc78f0624185783b522' +
      '83ca14a1', 'hex'),
    'sha-256, no label': Buffer.from(
      '0531eaeb4b8cef8eb77dd736d096b6dce840d164e9751f86a8d5ced0' +
      '9999506ffaf000eb6153f7456f311ae99e47f1207150eb97f2982db7' +
      '73be9e990e8a12985a5f115af906ef0c870eef3c9fca1c5b4d5e9904' +
      '99b67d0094adaf8debdf9a5d72335b54b3d1ca26ea0957d63e064ce9' +
      '69d52e53bd605c8fa9320d9eabe239eef47099555c194d1bb8dfeffe' +
      'ad6b4fd7f8f28990355cc8ee22a36c4867f0acead7f4a5025f1517f7' +
      '52a7e8c093533d0cd659ad60a7dc05044220019887016437dcc94c6f' +
      '9e8202b03bc955eb2c790d3fb7c7e77e2612ffa521daf467f640a749' +
      'e9e1191574be76e2d55c3cfe7a93551a7c28ddb2ba6b26c33a30c237' +
      '1cd8974d', 'hex'),
    'sha-384, no label': Buffer.from(
      '0c2392e30f92f1f4e4acd1b4a6fd99f983c61dcaf39bddde47b29ead' +
      '3add104a7a86df1f7099f3683c65affe329d2bcab95035ec96c13dab' +
      '9a0cb4b95960fee08aa5835566a1b57ddf545ac950509134ced00273' +
      '9ea6ff3e3de4841f9fa9061660ecf10533e9f50ca5164f324944cc7b' +
      '8e3aec6998a366f90cfaee7977651a0d1e8d194bcd1c2a008729aa01' +
      '1a9d0d8ca271f98e015bf466bcd99cd97686b592f66fb1369f54a358' +
      '937abcf917df6f39badc6f5ff630c7ac73b92fadbadd0c29fce04ca7' +
      'd62aab52b264e5a282bcbf02721c119e28e9426cd996b3791973d8a2' +
      'acf43a2c2f67ff884c1a77b83fc3268c640cab414336c31f7a6977e4' +
      '951031d4', 'hex'),
    'sha-512, no label': Buffer.from(
      '0626d346d5253113ddc0f8ced1918d88ebf00869f880af89c5e67bb3' +
      '794bb58a9bf619e5a55909418f6c7e14a858a0c530427502db7afe60' +
      '2493aa6f7ba83997198b0ae97ddb8d3a7dae5926dc190f0587452105' +
      '0a311cfd94fbd535df08b840b95ef9d3403583882002a33d4c09a2bd' +
      '50476ded93090b933dfe01b9d0e42cdb11a3efb8d4c5e5d223ec0475' +
      '25bba91af85fa0a5fc1566fd4973917c588f7980ec469065b536b340' +
      '39e899489e60f14fff5a43106e2bea9914394b5317b8d819d73409f1' +
      '7215dfb8b1f742620d0fafcc5251cd160f5c1c63baeaf12125d20f08' +
      'c51ed061072a33add5ab3b9a47354e58f4329d216f8fb93d5b76edf5' +
      '5d5b9c24', 'hex'),
    'sha-1, with label': Buffer.from(
      '450c932bdb5f223d1d40dabe85457d230849499a57c25bc9826ff356' +
      '5a7cfe82bb859e209fea96625bf678a639e96207a03a7d71354f02ca' +
      'd0687bc31b54fa6208a953cf5e1f611f5100f799d06340b9f4d5c23e' +
      'c8ab4ef50a3e90b0ce5e68ac2d3972c4f3a6224389294d0620e109ea' +
      'b72026281a5de6bf1bc0b743e09c40bd241bd8393aa430a44adaa7c4' +
      'd0dd4f6676177b1ae335b9c40ee99a068ce9cc996da3a4e2aacf4f7b' +
      '1abc817c6252ff5f8e471a05d7c681b36e82fbde8cd2e225c87564ac' +
      '1a8a610b57a168d244752e574afa9856c22a757afeebca96f93f6e6d' +
      '17c520515927d99ca34eedfd19bce31f23aebe159da0253c27c10b5c' +
      '0ffb7d97', 'hex'),
    'sha-256, with label': Buffer.from(
      'b4d46d087682180560797166729d951fea055f8ab80a10f9314e55de' +
      'e12fac6c2124ce2fd39fcaf134acdd1c6301b0335292be89a3cad44a' +
      '96cc3a1a2b36875bf6c93b6ab5090a76bf6a7a66af7202b692a9377c' +
      '54bbfeede1fc20c54f61dbfa3651347992c20dc458d40792ede81f71' +
      'd7c82a9eefb43398d6916a0b73a01547abbe2d19e51382d2343a0e37' +
      '52fad7c16eb28f65a33f95d8b0a39142ef3eccc3660e9d029b72e6e1' +
      '3779a7acb6bb4b9531d56890cae66f7d77ecd59b837a2b37f5b73c8c' +
      '67582d549df743f3a966b0e1c0b59afc5a5f11a17060c4696837065c' +
      'd4127f55be0c697bdb6e826fb320b751f6f0873b05d2ad0f66d7575f' +
      '8888ee0c', 'hex'),
    'sha-384, with label': Buffer.from(
      'cc3cbc830f7256f6be9bce3e44f9623fb29001f42af82f09fd088bba' +
      'd7b4bf5cf71392f241c369d38854e1ec4bcaf394c54473338741b43e' +
      '7b5b1b43850eb7413101065e72f94224fabd49da9bd5ccf067b9cebd' +
      '503cf9017fbe1cc4a7437bdecb7ccd99fbf24d97d5d7a2bd1f1cf401' +
      'a73a03a95d8f1b28a756e1553b5db052d1e0901523fcb66173c84675' +
      '6df0af66d0647ce6b4e89f4db04b8b3a39fe0db7191bf6b633abc5e2' +
      '1a0769e1ee933d446661f79527084446d7dccd4ac3b770985b467aa3' +
      '1e423334beccd1df6f432c120e6c9c3ea5dd06f694e99432d9f82c63' +
      'e976ebf84e2dca3dd3dcc14a06e5cbd47274f2d655a5c7727d350557' +
      'eed091b8', 'hex'),
    'sha-512, with label': Buffer.from(
      '8697b55eef5b0d5311398a15f2ce1856b8efee39e774718b0c806864' +
      '9339e4b7a7e129b4f7858d0079c1eba8b8f86b2222e961d7f7f014f5' +
      '0e00a711ebcc5161345109885b3b7ac8f94a3f440a12a2f30abe763c' +
      '184ae75c62b3dd9605424e5dc8d41d4c32f6be5407f5b09461051016' +
      'deada5c8a95d3a087be57cdc427b22453121196b20fa623d29de6c70' +
      '252ab2a3519d1ca00379580af9191916420024bbb0c79a7a8a2b48d9' +
      '5a2b7732d2a6ca0279ac18ac334aa12d6b96bb590959b7e9a9954e91' +
      'e49c8ad7e8db2121e0b2ae100648b9d622cc9fa19a0b978e27f44a4b' +
      'f3bf7be7203676eb0c13c8a5fca1572e6333f892b47a2cd267eda932' +
      '1cd27988', 'hex')
  };

  const passing = [
    {
      name: 'RSA-OAEP with SHA-1 and no label',
      publicKeyBuffer: spki,
      publicKeyFormat: 'spki',
      privateKey: null,
      privateKeyBuffer: pkcs8,
      privateKeyFormat: 'pkcs8',
      publicKey: null,
      algorithm: { name: 'RSA-OAEP' },
      hash: 'SHA-1',
      plaintext: plaintext.slice(0, 214),
      ciphertext: ciphertext['sha-1, no label']
    },
    {
      name: 'RSA-OAEP with SHA-256 and no label',
      publicKeyBuffer: spki,
      publicKeyFormat: 'spki',
      privateKey: null,
      privateKeyBuffer: pkcs8,
      privateKeyFormat: 'pkcs8',
      publicKey: null,
      algorithm: { name: 'RSA-OAEP' },
      hash: 'SHA-256',
      plaintext: plaintext.slice(0, 190),
      ciphertext: ciphertext['sha-256, no label']
    },
    {
      name: 'RSA-OAEP with SHA-384 and no label',
      publicKeyBuffer: spki,
      publicKeyFormat: 'spki',
      privateKey: null,
      privateKeyBuffer: pkcs8,
      privateKeyFormat: 'pkcs8',
      publicKey: null,
      algorithm: { name: 'RSA-OAEP' },
      hash: 'SHA-384',
      plaintext: plaintext.slice(0, 158),
      ciphertext: ciphertext['sha-384, no label']
    },
    {
      name: 'RSA-OAEP with SHA-512 and no label',
      publicKeyBuffer: spki,
      publicKeyFormat: 'spki',
      privateKey: null,
      privateKeyBuffer: pkcs8,
      privateKeyFormat: 'pkcs8',
      publicKey: null,
      algorithm: { name: 'RSA-OAEP' },
      hash: 'SHA-512',
      plaintext: plaintext.slice(0, 126),
      ciphertext: ciphertext['sha-512, no label']
    },
    {
      name: 'RSA-OAEP with SHA-1 and empty label',
      publicKeyBuffer: spki,
      publicKeyFormat: 'spki',
      privateKey: null,
      privateKeyBuffer: pkcs8,
      privateKeyFormat: 'pkcs8',
      publicKey: null,
      algorithm: { name: 'RSA-OAEP', label: new Uint8Array([]) },
      hash: 'SHA-1',
      plaintext: plaintext.slice(0, 214),
      ciphertext: ciphertext['sha-1, no label']
    },
    {
      name: 'RSA-OAEP with SHA-256 and empty label',
      publicKeyBuffer: spki,
      publicKeyFormat: 'spki',
      privateKey: null,
      privateKeyBuffer: pkcs8,
      privateKeyFormat: 'pkcs8',
      publicKey: null,
      algorithm: { name: 'RSA-OAEP', label: new Uint8Array([]) },
      hash: 'SHA-256',
      plaintext: plaintext.slice(0, 190),
      ciphertext: ciphertext['sha-256, no label']
    },
    {
      name: 'RSA-OAEP with SHA-384 and empty label',
      publicKeyBuffer: spki,
      publicKeyFormat: 'spki',
      privateKey: null,
      privateKeyBuffer: pkcs8,
      privateKeyFormat: 'pkcs8',
      publicKey: null,
      algorithm: { name: 'RSA-OAEP', label: new Uint8Array([]) },
      hash: 'SHA-384',
      plaintext: plaintext.slice(0, 158),
      ciphertext: ciphertext['sha-384, no label']
    },
    {
      name: 'RSA-OAEP with SHA-512 and empty label',
      publicKeyBuffer: spki,
      publicKeyFormat: 'spki',
      privateKey: null,
      privateKeyBuffer: pkcs8,
      privateKeyFormat: 'pkcs8',
      publicKey: null,
      algorithm: { name: 'RSA-OAEP', label: new Uint8Array([]) },
      hash: 'SHA-512',
      plaintext: plaintext.slice(0, 126),
      ciphertext: ciphertext['sha-512, no label']
    },
    {
      name: 'RSA-OAEP with SHA-1 and a label',
      publicKeyBuffer: spki,
      publicKeyFormat: 'spki',
      privateKey: null,
      privateKeyBuffer: pkcs8,
      privateKeyFormat: 'pkcs8',
      publicKey: null,
      algorithm: { name: 'RSA-OAEP', label },
      hash: 'SHA-1',
      plaintext: plaintext.slice(0, 214),
      ciphertext: ciphertext['sha-1, with label']
    },
    {
      name: 'RSA-OAEP with SHA-256 and a label',
      publicKeyBuffer: spki,
      publicKeyFormat: 'spki',
      privateKey: null,
      privateKeyBuffer: pkcs8,
      privateKeyFormat: 'pkcs8',
      publicKey: null,
      algorithm: { name: 'RSA-OAEP', label },
      hash: 'SHA-256',
      plaintext: plaintext.slice(0, 190),
      ciphertext: ciphertext['sha-256, with label']
    },
    {
      name: 'RSA-OAEP with SHA-384 and a label',
      publicKeyBuffer: spki,
      publicKeyFormat: 'spki',
      privateKey: null,
      privateKeyBuffer: pkcs8,
      privateKeyFormat: 'pkcs8',
      publicKey: null,
      algorithm: { name: 'RSA-OAEP', label },
      hash: 'SHA-384',
      plaintext: plaintext.slice(0, 158),
      ciphertext: ciphertext['sha-384, with label']
    },
    {
      name: 'RSA-OAEP with SHA-512 and a label',
      publicKeyBuffer: spki,
      publicKeyFormat: 'spki',
      privateKey: null,
      privateKeyBuffer: pkcs8,
      privateKeyFormat: 'pkcs8',
      publicKey: null,
      algorithm: { name: 'RSA-OAEP', label },
      hash: 'SHA-512',
      plaintext: plaintext.slice(0, 126),
      ciphertext: ciphertext['sha-512, with label']
    }
  ];

  const failing = [];

  return { passing, failing };
};
