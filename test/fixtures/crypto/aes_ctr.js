'use strict';

module.exports = function() {
  const kPlaintext =
    Buffer.from('546869732073706563696669636174696f6e206465736372696265' +
                '732061204a6176615363726970742041504920666f722070657266' +
                '6f726d696e672062617369632063727970746f6772617068696320' +
                '6f7065726174696f6e7320696e20776562206170706c6963617469' +
                '6f6e732c20737563682061732068617368696e672c207369676e61' +
                '747572652067656e65726174696f6e20616e642076657269666963' +
                '6174696f6e2c20616e6420656e6372797074696f6e20616e642064' +
                '656372797074696f6e2e204164646974696f6e616c6c792c206974' +
                '2064657363726962657320616e2041504920666f72206170706c69' +
                '636174696f6e7320746f2067656e657261746520616e642f6f7220' +
                '6d616e61676520746865206b6579696e67206d6174657269616c20' +
                '6e656365737361727920746f20706572666f726d20746865736520' +
                '6f7065726174696f6e732e205573657320666f7220746869732041' +
                '50492072616e67652066726f6d2075736572206f72207365727669' +
                '63652061757468656e7469636174696f6e2c20646f63756d656e74' +
                '206f7220636f6465207369676e696e672c20616e64207468652063' +
                '6f6e666964656e7469616c69747920616e6420696e746567726974' +
                '79206f6620636f6d6d756e69636174696f6e732e', 'hex');

  const kKeyBytes = {
    '128': Buffer.from('dec0d4fcbf3c4741c892dabd1cd4c04e', 'hex'),
    '256': Buffer.from('67693823fb1d58073f91ece9cc3af910e5532616a4d27b1' +
                       '3eb7b74d8000bbf30', 'hex')
  }

  const counter = Buffer.from('55aaf89ba89413d54ea727a76c27a284', 'hex');

  const kCiphertext = {
    '128': Buffer.from(
      'e91175fda4f5ea57c52b0d000bbe98af68c0a59058aeed8ab5b7063503a1ce4' +
      '70d79dad174f90aaafaa5449d848dc8b2c557d1e7fa4b9a41a2fb1e9fea1414' +
      'b593dab40c04f14b4f81400fe43c93990181b096a15561169aea177f100416e' +
      '20b6810b00ee1b04fef67f3bede28baf4d41d397daf1511e9020d7766e9e604' +
      '10de38e1432dbffa0f992dc1f0d4756544e8c765af7df706f90e009db9384c3' +
      '3e44dea543c2a77bbd52022de41e7d71a498de7feb9760eb47e503366c88dcc' +
      '2d1a387788de2d8f78e72c2bdd8815bc8a54e8d0eee275683ca5041290f031a' +
      'd5a4454efa17cc4907718f3ef4b75fedbd13583254f441a15a8a3323b12f40b' +
      '8fbebc816cf9b468d8d7a5a0fb548498c39a6ed84615f894929838aef8e3016' +
      '60f76b632493f23709fedfd5e107f78267f331b60a38c146f9710484a4acdef' +
      'f110b3b7745ff83aa8cb5de9e15b11e20a785572041f2852a1981156edcf07e' +
      '46eb64144449cce74b9cc94163a6fda8ae19219721d60b757b5b5ec718dabd5' +
      '0954b6e6a393f656f6346f40229d0c50e01c15701f2a4fe5d25a174edf9b90e' +
      'e0c0ebf9e06b5fe00558638a1ea3781403b0c9206d9e814d6a79fb7a56060e1' +
      'c7176af36c6a1ad635981a9bfd8007d8cf6d9f93f0e8e22b93a9a2ccd7090ab' +
      '1df63cea3f040', 'hex'),
    '256': Buffer.from(
      '37529a432f50ba4e53385f8266ec3deccceceade7ae29395e9291076c95bb9a' +
      '24f4792fcdd6ea5894b815edb5d5e4022fabe055a06b1a7e01979555b579838' +
      '64bf23019cb1b37ffdadb057f728cfb2af0a33d146344cfba0accb4dbf613a7' +
      'bee523ca6d6860e474a9c0f4d068d4c0acd94cc55cbf21e4285ca15116c9702' +
      '0f2c33b4585008f8fe97c9e29c0627c5d47c48d94be88b9b16c7f2df740a8d2' +
      'a07556305b82b919f7a87ca2ed19db27262c277c213f2a7eca25e5a6adbea43' +
      '0ba2e1061198171054285aff9e0869c638dcd524cbf1f255da675acad6d7867' +
      '9a9958b7a8f9bb21dd9c580ad196f9a0e4c6a6500d7bb21df74cd5934ce3c4d' +
      '8d1f39d34a2adb58d224c48097887cde9d3be146a3ea3bade4c6864cf9e445b' +
      '5c4c2b3ef4e2b8f5eea0ab1c0b9abe7a4fe5b2c0b1d94df6b12953d3273260e' +
      '80bd094dec97a3177a9cec0b5042be1804040c9439403b8f72f7426fa756ad6' +
      '266cf2c8659e740329dd0d24f9f85497662cad739f71d6174011c77f8f31fb4' +
      '4226288dfb86817ef17116321c71bb9ed97db6e990f62058580f006683431f2' +
      '29662f1d5e3cdaffe0335467ca72635688c939ec8b32d6465f651a635f73c0a' +
      '4e7f0aadb0e81f5bcbfaec2671ac97fdc2fd32f24c941775c37a6810d4b171b' +
      'c8aba90a86603', 'hex')
  };

  const kKeyLengths = [128, 256];

  const passing = [];
  kKeyLengths.forEach((keyLength) => {
    passing.push({
      keyBuffer: kKeyBytes[keyLength],
      algorithm: {name: 'AES-CTR', counter, length: 64},
      plaintext: kPlaintext,
      result: kCiphertext[keyLength]
    });
  });

  const failing = [];
  kKeyLengths.forEach((keyLength) => {
    failing.push({
      keyBuffer: kKeyBytes[keyLength],
      algorithm: {name: 'AES-CTR', counter, length: 0},
      plaintext: kPlaintext,
      result: kCiphertext[keyLength]
    });

    failing.push({
      keyBuffer: kKeyBytes[keyLength],
      algorithm: {name: 'AES-CTR', counter, length: 129},
      plaintext: kPlaintext,
      result: kCiphertext[keyLength]
    });
  });

  return { passing, failing, decryptionFailing: [] };
};
