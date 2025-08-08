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

  const kKeyBytes = Buffer.from('67693823fb1d58073f91ece9cc3af910e5532616a4d27b1' +
                       '3eb7b74d8000bbf30', 'hex')

  const iv = Buffer.from('3a92732aa6ea39bf3986e0c73', 'hex');

  const additionalData = Buffer.from(
    '5468657265206172652037206675727468657220656469746f72696' +
    '16c206e6f74657320696e2074686520646f63756d656e742e', 'hex');

  const tag = Buffer.from('87d611a2b8012f4eb792ccdee7998d22', 'hex')

  const tag_with_empty_ad = Buffer.from('2ba3e8380c1f49f10665fd15a4ac599e', 'hex')

  const kCiphertext = Buffer.from(
    '01e15951ce23d7672df9d13f19c54ff5b3fe17114eb637ec25c1a8ac2' +
    '24eebe154b3a1206187e18abd31d022b1a66551fbbf0ae2d9fa4e9ab4' +
    'a680b185528000a7654731f05f405ce164cfc904d1759afa758ac459f' +
    'e26420fccac9692af9259243f53e7e0f42d56c6a4b4827056ca76bc9d' +
    'e92577a9f405810fd1e4cb7289f7d528772bde654fef456f031b87802' +
    '6616df7349bef4fdff4a52953afadddbd61a3bd1a43815daf1b1ab962' +
    '8aaeaee52866466dcb45650b489b2226a01da24d85c20af24e2beb790' +
    '233081c5651258cf77e5c47e87ac070aeaa470d13b28b4df82729c350' +
    '3cd80a65ac50a8d7a10dabe29ac696410b70209064c3b698343f97f5a' +
    '38d63265504ee0922cf5a7c03fe0f3ac1fce28f8eed0153d2f6c500ef' +
    '68c71e56e3f1abbfc194be4dd75b73983c3e7c0c68555b71eb4695110' +
    'bb8cd8f495ce7c1e4512c72fca23a095897a9a0dfd584abc3e949cf3e' +
    '0fa1d855284d74a915b6e7455e0307985a356c01878700b21c6e0afac' +
    'ee72021a81c3164193e0126d5b841018da2c7c9aa0afb8cd746b378e3' +
    '04590eb8b0428b4409b7bcb0cb4a5e9072bb693f011edbe9ab6c5e0c5' +
    'ca51f344fb29034cdbe78b3b66d23467a75e5d28f7e7c92e4e7246ba0' +
    'db7aa408efa3b33e57a4d67fda86d346fc690f07981631', 'hex');

  const kTagLengths = [128];

  const passing = [];
  kTagLengths.forEach((tagLength) => {
    const byteCount = tagLength / 8;
    const result =
      new Uint8Array(kCiphertext.byteLength + byteCount);
    result.set(kCiphertext, 0);
    result.set(tag.slice(0, byteCount),
                kCiphertext.byteLength);
    passing.push({
      keyBuffer: kKeyBytes,
      algorithm: { name: 'ChaCha20-Poly1305', iv, additionalData, tagLength },
      plaintext: kPlaintext,
      result
    });

    const noadresult =
      new Uint8Array(kCiphertext.byteLength + byteCount);
    noadresult.set(kCiphertext, 0);
    noadresult.set(tag_with_empty_ad.slice(0, byteCount),
                    kCiphertext.byteLength);
    passing.push({
      keyBuffer: kKeyBytes,
      algorithm: { name: 'ChaCha20-Poly1305', iv, tagLength },
      plaintext: kPlaintext,
      result: noadresult
    });
  });

  const failing = [];
  [24, 48, 72, 95, 129].forEach((badTagLength) => {
    failing.push({
      keyBuffer: kKeyBytes,
      algorithm: {
        name: 'ChaCha20-Poly1305',
        iv,
        additionalData,
        tagLength: badTagLength
      },
      plaintext: kPlaintext,
      result: kCiphertext
    });
  });

  return { passing, failing, decryptionFailing: [] };
};
