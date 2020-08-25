'use strict';

module.exports = function() {
  const pkcs8 = Buffer.from(
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
    '3ea7e2587876', 'hex');

  const spki = Buffer.from(
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
    '8b641d7', 'hex');

  const plaintext = Buffer.from(
    '5f4dba4f320c0ce876725afce5fbd25bf83e5a7125a08cafe73c3ebac421779df9d55' +
    'd180c3ae9942645e1d82fee8c9d294b3cb1a08a9931201b3c0e81fc47cacf8315a2af' +
    '66324113c3b66230c34608c4f4593634ce02b267362277f0a840ca74bc3d1a6236952' +
    'c5ed7aaf8a8fecbddfa7584e6978cea5d2a5b9fb7f1b48c8b0be58a305202754d8376' +
    '107374793cf026aaee5300727d836cd71e71b345ddb2e44446ffc5b901635413890d9' +
    '10ea380984a90191031323f16dbcc9d6be168b84885384ca03e12600ac1c248028af3' +
    '726cc93463882ea8c02aab', 'hex');

  const signatures = {
    'sha-1': Buffer.from(
      '303d021c685192dc48fbd8c6dd80536eaf92050ca07aa85af5a25a41a93bc8ea021' +
      'd009a032a19182ceca7b202d7e0ce327fe5830a9937a37714d4c6801ae9', 'hex'),
    'sha-256': Buffer.from(
      '303d021c7b660d1b2b40237d0f615877efbf2a3f51577bc42a3cb4e280c0a6a9021' +
      'd008393a01a9bcabce267dfcc6bc867d348911f6eda3db96d3078a380c1', 'hex'),
    'sha-384': Buffer.from(
      '303e021d00ab84d3c306aea1c5e69e126d21dafc03ee9c7aa8fa38884de5015d510' +
      '21d0099030adacdc6787dcb1dc3b05fa122ad72d4ddc281cc619565dba947', 'hex'),
    'sha-512': Buffer.from(
      '303c021c069c40654607388a56d3cb827c11121d07850b3b62ed203c1d92ce7f021' +
      'c0e69f41933d0ee3b1c1b184d742f3813ee3b32d053f13fbb93ab8229', 'hex'),
  }

  const vectors = [
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'NODE-DSA' },
      hash: 'SHA-1',
      plaintext,
      signature: signatures['sha-1']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'NODE-DSA' },
      hash: 'SHA-256',
      plaintext,
      signature: signatures['sha-256']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'NODE-DSA'},
      hash: 'SHA-384',
      plaintext,
      signature: signatures['sha-384']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'NODE-DSA' },
      hash: 'SHA-512',
      plaintext,
      signature: signatures['sha-512']
    }
  ];

  return vectors;
};
