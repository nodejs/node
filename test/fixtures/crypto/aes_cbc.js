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

  const iv = Buffer.from('55aaf89ba89413d54ea727a76c27a284', 'hex');

  const kCipherText = {
    '128': Buffer.from(
      '237f03fee70872e78faec148ddbd01bd77cb96e3381ef4ece2afea17a7afd37' +
      'ccbe461df9c4d58aea6bbbae1b05cfab1e129877cd756c6867c319a3ce05da5' +
      '0cbef5f1a4f7dce345f269d06cdec1df00e2d927a04e93bf2699e8ceddfe19b' +
      '9f907b5d76862a3c2a167a1eda70af2255002ffad60146aaa6e5026887f1055' +
      'f44eac386a0373823aba81ecfffbb270189f52fc01b2845c287d12877440b21' +
      'fae577272da4e6f00effc4f3f773a764e37f92482e1cd0d4c61d6faaee84367' +
      'd3b2ce2081bcf364473f9a9fc87d228a2749824b61cbcc6ff44bbab52bcfaf9' +
      '262cf1b175a90a113ebc75d62ee48869ddccf42a7ec5e390003cafa371aa314' +
      '85bf43143f96cb57d82c39bcec40506f441a0c0aa35203bf1347bac4b154f40' +
      '74e29accb1be1e76cce8dddfdccdc8614823671517fc51b65799fdfc173be0c' +
      '99aee7c45c8e9c3dbd031299cebe3aff9a7342176b5edc9cdce4f14206b82ce' +
      'ef933f06d8ed0bd0b7546aad9aad842e712af79dd101d8b37675bef6f1d6c5e' +
      'b38a8649821d45b6c0f996a54f2f5bcbe23f57343cacbfbeb3ab9bcd58ac6f3' +
      'b28c6fad194b173c8282ba5a74374409ff051fdeb898431dfd6ac35072fb8df' +
      '783b33217c93dd1b3c10fe187373d64b496188d6d1b16a47fed35e3968aaa82' +
      '3255dcbc7261c54', 'hex'),
    '256': Buffer.from(
      '29d5798cb5e3c86164853ae36a73193f4d331a39ee8c633f47d38054731aec3' +
      '46751910e65a1b53a87c138a7d6dc053455deb71b6586569b40947cd4dbfb41' +
      '2a202c80023280dd16ee38bd531c7a799dd7879780e9c141be5694bf8cc4780' +
      '8ac64a6fe29f54b3806a6f4b26fea17046b061684bbe61147ac71ee4904b45a' +
      '674d2533767081eec707de7aad1ee8b2e9ea90620eea704d443e3e9fe665622' +
      'b02cc459c566880228007ad5a7821683b2dfb5d33f0e83c5ebd865a14b87a1d' +
      'e155d526749f50456aa8ecc9458c62f02da085e16a2df5d4a0b0801b7299b69' +
      '091d648c48ab7573df59638529ee032727d7aaca181ea463ff5881e880980dc' +
      'e59ddec395bd46084728c35d1b07eaa4af66c99573f8b37d427ac21a3ddac6b' +
      '5988cc730941f0ef1c5034680ef20560fd756f5be5f8d296f00e81c984357c5' +
      'ff760dfb475416e786bcaf738a25c705eec70263cb4b3ee71596ef5ec9b9db3' +
      'ad2e497834c94683c4a5206a831fbb603e8add2c91365a6075e0bc2d392e54b' +
      'f10f32bb24af4ee362e0035fd15d7e70b21d126cf1e84fd22902eed0beab869' +
      '3bcbfe57a20d1a67681df82d6c359435eda9bb90090ff84d5193b53f2394594' +
      '6d853da31ed6fe36a903d94d427bc1ccc76d7b31badfe508e6a4abc491e10a6' +
      'ff86fa4d836e1fd', 'hex')
  };

  const kBadPadding = {
    '128': {
      zeroPadChar: Buffer.from('ee1bf8a9da8aa456cf6624df06a64d0e', 'hex'),
      bigPadChar: Buffer.from('5b437768fceeaf90114b0ca3d4342e33', 'hex'),
      inconsistentPadChars:
        Buffer.from('876570d0036ae21419db4f5e3ad4f2c0', 'hex')
    },
    '256': {
      zeroPadChar: Buffer.from('01fd8dd61ec1fe448cc89d6ec859b181', 'hex'),
      bigPadChar: Buffer.from('58076edd4a22616d6319bdde5e5a1b3c', 'hex'),
      inconsistentPadChars:
        Buffer.from('98363c943b88c1154d8caa43784a6a3e', 'hex')
    }
  };

  const kKeyLengths = [128, 256];

  const passing = [];
  kKeyLengths.forEach((keyLength) => {
    passing.push({
      keyBuffer: kKeyBytes[keyLength],
      algorithm: { name: 'AES-CBC', iv },
      plaintext: kPlaintext,
      result: kCipherText[keyLength]
    });
  });

  const failing = [];
  kKeyLengths.forEach((keyLength) => {
    failing.push({
      keyBuffer: kKeyBytes[keyLength],
      algorithm: {
        name: 'AES-CBC',
        iv: iv.slice(0, 8)
      },
      plaintext: kPlaintext,
      result: kCipherText[keyLength]
    });

    const longIv = new Uint8Array(24);
    longIv.set(iv, 0);
    longIv.set(iv.slice(0, 8), 16);
    failing.push({
        keyBuffer: kKeyBytes[keyLength],
        algorithm: { name: 'AES-CBC', iv: longIv },
        plaintext: kPlaintext,
        result: kCipherText[keyLength]
    });
  });

  // Scenarios that should fail decryption because of bad padding
  const decryptionFailing = [];
  kKeyLengths.forEach(function(keyLength) {
    [
      'zeroPadChar',
      'bigPadChar',
      'inconsistentPadChars'
    ].forEach((paddingProblem) => {
      const badCiphertext =
        new Uint8Array(kCipherText[keyLength].byteLength);
      badCiphertext.set(
        kCipherText[keyLength]
          .slice(0, kCipherText[keyLength].byteLength - 16));
      badCiphertext.set(kBadPadding[keyLength][paddingProblem]);

      decryptionFailing.push({
        keyBuffer: kKeyBytes[keyLength],
        algorithm: { name: 'AES-CBC', iv },
        plaintext: kPlaintext,
        result: badCiphertext
      });
    });
  });

  return { passing, failing, decryptionFailing };
};
