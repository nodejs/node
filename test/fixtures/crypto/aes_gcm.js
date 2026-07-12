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

  const iv = Buffer.from('3a92732aa6ea39bf3986e0c73fa920002' +
                         '02175385ef8adeac2c87335eb928dd4', 'hex');

  const additionalData = Buffer.from(
    '5468657265206172652037206675727468657220656469746f72696' +
    '16c206e6f74657320696e2074686520646f63756d656e742e', 'hex');

  const tag = {
    '128': Buffer.from('c2e2c6fdef1cc5f07bd8b097efc8b8b7', 'hex'),
    '256': Buffer.from('bceff1309f15d500f12a554cc21c313c', 'hex')
  };

  const tag_with_empty_ad = {
    '128': Buffer.from('de330b1724defaf81b621e519623dcc6', 'hex'),
    '256': Buffer.from('f4ba56cb9a25bff8f6398b82e02fd9ee', 'hex')
  };

    // AES-GCM produces ciphertext and a tag.
  const kCiphertext = {
    '128': Buffer.from(
      'b4f128b7693493eee0afafeca8f4f17909cae1ed38d8fdfeba666fcfe4be82b19ff6' +
      '0635f971e4fe517efdbf642bfb936b5ba6e7c9f1b4d6702f7ba4ba86364116b5c952' +
      'ec3b348bac2729597b3e66a75296fa5d60a98759f5ffa4c0a99f19108b914c049083' +
      '94c5cc2e176ec1e47f78f21836f0b5a262f4f944867a7e97266c7444966d26c2159f' +
      '8ccdb7236197ba789116eb16d2dfbb8fa2b75dc468336035eafab84ced9d25cbe257' +
      'de4bf05fdade4051a54bc9d8be0d74d945422fa144f74afd9db5a27935205b7ce669' +
      'e011bb323d4d674f4739a374ea951b69181f9f0380822a5e7dc88efb94c91195e854' +
      '321112cbbae2a4e3ca4c4110a3e084341f658148ab9f2ab1fd6256c95f753e0ccd4e' +
      '247ec47959b925a142b575ba477c846e781bf6a3120d5ac87f52d1f1aa49f78960f4' +
      'fefb77479c1b6b35212d16009030200b74157df6d9ab9ee08eea8df2a8599a42e3a1' +
      'b66001584e0c07ef1ece1f596f6b2a25f194e80108fb7592b70930275e3b46e61aa5' +
      '619c8c8d1f3e0ace3730cf00c5cac56c85af5004109adfff04c4bcb2f01d0d7805e1' +
      'ca0323e19e5c9849cd6b9de0f563c2ab9cf5f7b7a5283ec86e1d97ce64af5824f25a' +
      '045249fa8cf5d9099923f2ce4ec579730f508065bff05b97f93e3ef412031187ded2' +
      '5d957b', 'hex'),
    '256': Buffer.from(
      '0861eb7146208783d2d17ca0ffb6091d7dc11bf0812e0289a98e3d079136aacf9f6f' +
      '275f573fa21b0612dbd774225a3972f4669143063398f7a5f27464dbb148b1116e43' +
      '5ddb64d914cf599a2d25695343a28ceb8128b1caae3694379cc1e8f986a3c3337274' +
      '4126496360f9e0451177babcb52b4e9c4c8ae23f05f8095e1a0102eb27ae4a2fb716' +
      '282f2f0d64770c43b2b838a7ee8f0d2cd0b9976c0611347ab6d2cf2adb254a5e7e24' +
      'f9252004da2cee4538db1f4dad2ebb672470d5fc2857a4f0a39f20817db26c2f1c1f' +
      '242a73240e91c39cbf2ea3f9b51f5a491e4839df3f3c4f8c0e751f91de9c79ed2091' +
      '8f600cfe2315153ba8ab9ad9003bcaaf67d6c0af1a122b36b0de4b16077afde0913d' +
      '2ad049ed548dd1d5e42ef43b0944062358bd0a3e09551c2c521399a0b2f038a0f4c9' +
      'ad4d3d14e31eb4a71069b9c15fcf2917864ec6b65d1859f7e74be9c289f272c2be82' +
      '8aee5e89c1c27389becfa9539b0ed2a081c3a1eaddff7243620c5d2941b7f467f765' +
      '52f67d577d4e15ba66cd142820c9ae0f34f0d9b4a26c06d3291287e8b812bca99dbe' +
      '4ca64bb07f27fb16cb995031f17c89977bcc2b9fbeb1c41275a92e98fb2d19a41b91' +
      'd6e4370f0283d850ffccaf643b910f6728212dffc8feac8a143a57b6c094db2958e6' +
      'e546f9', 'hex')
    };

  const kKeyLengths = [128, 256];
  const kTagLengths = [32, 64, 96, 104, 112, 120, 128];

  const passing = [];
  kKeyLengths.forEach((keyLength) => {
    kTagLengths.forEach((tagLength) => {
      const byteCount = tagLength / 8;
      const result =
        new Uint8Array(kCiphertext[keyLength].byteLength + byteCount);
      result.set(kCiphertext[keyLength], 0);
      result.set(tag[keyLength].slice(0, byteCount),
                 kCiphertext[keyLength].byteLength);
      passing.push({
        keyBuffer: kKeyBytes[keyLength],
        algorithm: { name: 'AES-GCM', iv, additionalData, tagLength },
        plaintext: kPlaintext,
        result
      });

      const noadresult =
        new Uint8Array(kCiphertext[keyLength].byteLength + byteCount);
      noadresult.set(kCiphertext[keyLength], 0);
      noadresult.set(tag_with_empty_ad[keyLength].slice(0, byteCount),
                     kCiphertext[keyLength].byteLength);
      passing.push({
        keyBuffer: kKeyBytes[keyLength],
        algorithm: { name: 'AES-GCM', iv, tagLength },
        plaintext: kPlaintext,
        result: noadresult
      });
    });
  });

  const failing = [];
  kKeyLengths.forEach((keyLength) => {
    [24, 48, 72, 95, 129].forEach((badTagLength) => {
      failing.push({
        keyBuffer: kKeyBytes[keyLength],
        algorithm: {
          name: 'AES-GCM',
          iv,
          additionalData,
          tagLength: badTagLength
        },
        plaintext: kPlaintext,
        result: kCiphertext[keyLength]
      });
    });
  });

  return { passing, failing, decryptionFailing: [] };
};
