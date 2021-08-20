'use strict';

module.exports = function() {
  const pkcs8 = {
    'P-384': Buffer.from(
      '3081b6020100301006072a8648ce3d020106052b8104002204819e30819b02010104' +
      '3002a9a0d899efa87e7564110907e9d82c21bd6265a37abd9a6fdb0f80ec844dd3a1' +
      '425320d67ddc30f5db74efb9a2e661a164036200041d319d692dca5f5754ba7b32c1' +
      '1642c6d8d2b4fb8249c3f214d71e90b5252966d97f7beb1faab1e4f3e2605549c2ee' +
      'db520329b3bea6b5e55624a15150a16966635f1916ef04dd758e69409d0633cb4b25' +
      '994179b22a769c743436910e799951', 'hex'),
    'P-521': Buffer.from(
      '3081ee020100301006072a8648ce3d020106052b810400230481d63081d302010104' +
      '4201533e618f98ead1b513ec8878c8820d377a36d8f03f2ba046c931825a3d358730' +
      'c0b26033dbb7f7e4a3d4434a035e24b707f9124766176e1af0b85df22eaaba9c25a1' +
      '8189038186000400a6deecfb489117f1e41cc4a064073d8673086e51db25086e8db7' +
      '64d4eff60aad6358fdcf967ac68459275e2a804f8eeeb7e4c4284b1451c0a5ea76fe' +
      '7007ac054701c5eddaf9a89e7c4fdcc924c737d8f585da9703a954c23be7c14aafa6' +
      '6654b256770a938e7f26e700c603931c7bd0bdb5d0632c7d1eab466f09d976c24a32' +
      '3e1b7c', 'hex')
  }

  const spki = {
    'P-384': Buffer.from(
      '3076301006072a8648ce3d020106052b81040022036200041d319d692dca5f5754ba' +
      '7b32c11642c6d8d2b4fb8249c3f214d71e90b5252966d97f7beb1faab1e4f3e26055' +
      '49c2eedb520329b3bea6b5e55624a15150a16966635f1916ef04dd758e69409d0633' +
      'cb4b25994179b22a769c743436910e799951', 'hex'),
    'P-521': Buffer.from(
      '30819b301006072a8648ce3d020106052b81040023038186000400a6deecfb489117' +
      'f1e41cc4a064073d8673086e51db25086e8db764d4eff60aad6358fdcf967ac68459' +
      '275e2a804f8eeeb7e4c4284b1451c0a5ea76fe7007ac054701c5eddaf9a89e7c4fdc' +
      'c924c737d8f585da9703a954c23be7c14aafa66654b256770a938e7f26e700c60393' +
      '1c7bd0bdb5d0632c7d1eab466f09d976c24a323e1b7c', 'hex')
  }

  const plaintext = Buffer.from(
    '5f4dba4f320c0ce876725afce5fbd25bf83e5a7125a08cafe73c3ebac421779df9d55d' +
    '180c3ae9942645e1d82fee8c9d294b3cb1a08a9931201b3c0e81fc47cacf8315a2af66' +
    '324113c3b66230c34608c4f4593634ce02b267362277f0a840ca74bc3d1a6236952c5e' +
    'd7aaf8a8fecbddfa7584e6978cea5d2a5b9fb7f1b48c8b0be58a305202754d83761073' +
    '74793cf026aaee5300727d836cd71e71b345ddb2e44446ffc5b901635413890d910ea3' +
    '80984a90191031323f16dbcc9d6be168b84885384ca03e12600ac1c248028af3726cc9' +
    '3463882ea8c02aab', 'hex');

  // For verification tests.
  const signatures = {
    'P-384': {
      'SHA-1': Buffer.from(
        '65fe070ec3eac35250d00b9ee6db4d2dadd5f3bbb9c495c8671d2a0d2b99149fb2' +
        '4f88af074e0b903268b3d0ed5f0e14685796b28fe34b2d8edcdf10845b24cf79b3' +
        '3627d8bd2c81621cb51e030c21a43abb0a8740fac26f8522e683c367ac96', 'hex'),
      'SHA-256': Buffer.from(
        '4bc2dfea3bcda4fbb4fd927b030f9b80b1f5d2ad9bb7aa06293869577120b2b1d0' +
        'ef11ccd9fed0714aab36bef63928f784f53c7e09df93e9b3e5b0c883cf720951b4' +
        'fe2382c7842edcfcd45d956a72d29a4030a038a900e6f7dd857a5650d3e8', 'hex'),
      'SHA-384': Buffer.from(
        '0dd9c2c7f0b6f4d9328254a902e87374b3c092195e6be21aa1a6dcd8eba60f7b0b' +
        '38c4006dfa2146d4e9fd23dc336179974017493a1f4f74eecfe455be3da9ed9964' +
        '1d81610dfeb468b607da941d5714e7b51aee2c45aa0e9c4da021b2370090', 'hex'),
      'SHA-512': Buffer.from(
        '72fbdb369fd34c1c54264d07f4facd69b02e4206f8a8bb259b882a305c56fde2d3' +
        '5107e493c53cd6b4af0b31306f4d03fd43cfc762a1030e17a3d775453a1212b142' +
        '9f7b3d93066a5f42a10b138cd177dc09616e827d598822d78d4627b754e6', 'hex')
    },
    'P-521': {
      'SHA-1': Buffer.from(
        '01781a17a60e43126960fd396e1210916c2115ca4428d968389c4b46c1553674ce' +
        '937b8e21700ce60932ae0f575ca187dd597720db839eb1f20c7e3394787559dcd5' +
        '00207e570df5c7e4ad9fc0a5f72065e9ce1c9e3d12ca5e6dd9f44fe128561b75f4' +
        '226c4fadf23d83536cc669ea4098e373b6cb919c8b5cfc0505a67d96b276a46a3d',
        'hex'),
      'SHA-256': Buffer.from(
        '0174dba77b14d73f66f5716786a3e5a8d7c931445e6d320a9229d961d8a1b3efd1' +
        '1a5ea33c79495ac599bbb68a641a849d58d83ef854cc265fa6c917dff6ee435a67' +
        '01b3d5527dac20fb0a7033c3fe79744eacef7b3ffc27b64dc863f86f42982cb222' +
        '9245fe9de48aa59eb653d4497086d911a5bd270e95c51e7e98f7a5863fc7fb065c',
        'hex'),
      'SHA-384': Buffer.from(
        '01f77db1e51378e117c5b8bec8a03f9657d244c54e837908bf7101255f4151525d' +
        '9e89cf7f54631b3368919d3824ff9f7f78fe81239a1a9fde2b7a83e95ca6a0ca11' +
        '01b98b1da4ed00ec769367e9958b8047d47f92ab8bff96f1330bf948c92209011b' +
        '8cdbb496d464dbb916720eb702bdad928c99b980b76504e0ad1c12b4a85731c70c',
        'hex'),
      'SHA-512': Buffer.from(
        '00b2caaf6798519a9d36dbfafe786b2fba1cc2acb99593c177b36e3a1ceeb70227' +
        '5ae23cfcca0aad78f6b6dee6b4718b95d0d1a715aa3378470e50b516c18e0f3305' +
        '01f0071e6a32867fa70f695cd39c4e87e142b9e4134d38740bd6fee354a575167e' +
        '13524e94832637910fe11e53a85fb21b91adb81bb1779c4e2b8bc87c717dc35084',
        'hex')
    }
  }

  const curves = ['P-384', 'P-521'];
  const hashes = ['SHA-1', 'SHA-256', 'SHA-384', 'SHA-512'];

  const vectors = [];
  curves.forEach((namedCurve) => {
    hashes.forEach((hash) => {
      vectors.push({
        publicKeyBuffer: spki[namedCurve],
        privateKeyBuffer: pkcs8[namedCurve],
        name: 'ECDSA',
        namedCurve,
        hash,
        plaintext,
        signature: signatures[namedCurve][hash]
      });
    })
  });

  return vectors;
}
