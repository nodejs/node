'use strict';

module.exports = function() {
  const pkcs8 = Buffer.from(
    '308204bf020100300d06092a864886f70d0101010500048204a9308204a5020100028' +
    '2010100d3576092e62957364544e7e4233b7bdb293db2085122c479328546f9f0f712' +
    'f657c4b17868c930908cc594f7ed00c01442c1af04c2f678a48ba2c80fd1713e30b5a' +
    'c50787ac3516589f171967f6386ada34900a6bb04eecea42bf043ced9a0f94d0cc09e' +
    '919b9d7166c08ab6ce204640aea4c4920db6d86eb916d0dcc0f4341a10380429e7e10' +
    '32144ea949de8f6c0ccbf95fa8e928d70d8a38ce168db45f6f134663d6f656f5ceabc' +
    '725da8c02aabeaaa13ac36a75cc0bae135df3114b66589c7ed3cb61559ae5a384f162' +
    'bfa80dbe4617f86c3f1d010c94fe2c9bf019a6e63b3efc028d43cee611c85ec263c90' +
    '6c463772c6911b19eec096ca76ec5e31e1e3020301000102820101008b375ccb87c82' +
    '5c5ff3d53d009916e9641057e18527227a07ab226be1088813a3b38bb7b48f3770551' +
    '65fa2a9339d24dc667d5c5ba3427e6a481176eac15ffd49068311e1c283b9f3a8e0cb' +
    '809b4630c50aa8f3e45a60b359e19bf8cbb5ecad64e761f1095743ff36aaf5cf0ecb9' +
    '7fedaddda60b5bf35d811a75b822230cfaa0192fad40547e275448aa3316bf8e2b4ce' +
    '0854fc7708b537ba22d13210b09aec37a2759efc082a1531b23a91730037dde4ef26b' +
    '5f96efdcc39fd34c345ad51cbbe44fe58b8a3b4ec997866c086dff1b8831ef0a1fea2' +
    '63cf7dacd03c04cbcc2b279e57fa5b953996bfb1dd68817af7fb42cdef7a5294a57fa' +
    'c2b8ad739f1b029902818100fbf833c2c631c970240c8e7485f06a3ea2a84822511a8' +
    '627dd464ef8afaf7148d1a425b6b8657ddd5246832b8e533020c5bbb568855a6aec3e' +
    '4221d793f1dc5b2f2584e2415e48e9a2bd292b134031f99c8eb42fc0bcd0449bf22ce' +
    '6dec97014efe5ac93ebe835877656252cbbb16c415b67b184d2284568a277d5933558' +
    '5cfd02818100d6b8ce27c7295d5d16fc3570ed64c8da9303fad29488c1a65e9ad711f' +
    '90370187dbbfd81316d69648bc88cc5c83551afff45debacfb61105f709e4c30809b9' +
    '0031ebd686244496c6f69e692ebdc814f64239f4ad15756ecb78c5a5b09931db18307' +
    '7c546a38c4c743889ad3d3ed079b5622ed0120fa0e1f93b593db7d852e05f02818038' +
    '874b9d83f78178ce2d9efc175c83897fd67f306bbfa69f64ee342368ced47c80c3f1c' +
    'e177a758d64bafb0c9786a44285fa01cdec3507cdee7dc9b7e2b21d3cbbcc100eee99' +
    '67843b057329fdcca62998ed0f11b38ce8b0abc7de39017c71cfd0ae57546c559144c' +
    'dd0afd0645f7ea8ff07b974d1ed44fd1f8e00f560bf6d45028181008529ef9073cf8f' +
    '7b5ff9e21abadf3a4173d3900670dfaf59426abcdf0493c13d2f1d1b46b824a6ac189' +
    '4b3d925250c181e3472c16078056eb19a8d28f71f308092753481d49444fdf78c9ea6' +
    'c24407dc018e77d3afef385b2ff7439e96237941332dd446cebeffdb4404fe4f71595' +
    '161d016402c334d0f57c61abe4ff9f4cbf90281810087d87708d46763e4ccbeb2d1e9' +
    '712e5bf0216d70de9420a5b2069b7459b99f5d9f7f2fad7cd79aaee67a7f9a34437e3' +
    'c79a84af0cd8de9dff268eb0c4793f501f988d540f6d3475c2079b8227a23d968dec4' +
    'e3c66503187193459630472bfdb6ba1de786c797fa6f4ea65a2a8419262f29678856c' +
    'b73c9bd4bc89b5e041b2277', 'hex');

  const spki = Buffer.from(
    '30820122300d06092a864886f70d01010105000382010f003082010a0282010100d35' +
    '76092e62957364544e7e4233b7bdb293db2085122c479328546f9f0f712f657c4b178' +
    '68c930908cc594f7ed00c01442c1af04c2f678a48ba2c80fd1713e30b5ac50787ac35' +
    '16589f171967f6386ada34900a6bb04eecea42bf043ced9a0f94d0cc09e919b9d7166' +
    'c08ab6ce204640aea4c4920db6d86eb916d0dcc0f4341a10380429e7e1032144ea949' +
    'de8f6c0ccbf95fa8e928d70d8a38ce168db45f6f134663d6f656f5ceabc725da8c02a' +
    'abeaaa13ac36a75cc0bae135df3114b66589c7ed3cb61559ae5a384f162bfa80dbe46' +
    '17f86c3f1d010c94fe2c9bf019a6e63b3efc028d43cee611c85ec263c906c463772c6' +
    '911b19eec096ca76ec5e31e1e30203010001', 'hex');

  const plaintext = Buffer.from(
    '5f4dba4f320c0ce876725afce5fbd25bf83e5a7125a08cafe73c3ebac421779df9d55' +
    'd180c3ae9942645e1d82fee8c9d294b3cb1a08a9931201b3c0e81fc47cacf8315a2af' +
    '66324113c3b66230c34608c4f4593634ce02b267362277f0a840ca74bc3d1a6236952' +
    'c5ed7aaf8a8fecbddfa7584e6978cea5d2a5b9fb7f1b48c8b0be58a305202754d8376' +
    '107374793cf026aaee5300727d836cd71e71b345ddb2e44446ffc5b901635413890d9' +
    '10ea380984a90191031323f16dbcc9d6be168b84885384ca03e12600ac1c248028af3' +
    '726cc93463882ea8c02aab', 'hex');

  const signatures = {
    'sha-1, no salt': Buffer.from(
      '1f1cd81ecb3bb31df2e5f0f64c5c0a310c7cf88d19eb512a5078e156d823727af88' +
      '68a12e5dfbfa976386784ba982c39928789b134952a4a28c6241177bcf2248f2adb' +
      '60077f545dd17e4f809b3b859fd430d1681e8047126d77369519eed5b618f3297a5' +
      '75085f0c931ed248cf60bbd7efffa0a8c2b874ba7f81ecd6bf391d01f1e881d827a' +
      '7b95df874d9adabb7b07f131ab33142a8b0b6d5ca9685671d49b982b67651909eaa' +
      '17b96b393e04fb36d972f9b258f1b79123df212d39924a4deaec506cf640f1dedd0' +
      '2d28845f3548d8488652788e2e2146f3ce8a86a556d84b4578f10da29abdb176a68' +
      '718cc1b2270b0735c2e5ca6c6bb0afac23a5bfa817a', 'hex'),
    'sha-256, no salt': Buffer.from(
      '6157d668ed655d978b4c158c8419eb80718dfdfc7d4b34357f9917e9e116b6f3b65' +
      '040c9d16155c081d6887abcb3ba4ffa0191e4807ee206681aa1d4809ea20de5186b' +
      '77e3caced07fc9b3d71b9df0ac81b5c3273ff3f74f32a7ad34c65062a31540ced30' +
      '527efa4b7aa2d27ff7f80535f3e65ce352eb9e18b5054416de959354a4dcccb2542' +
      'e33a8358eda620a8653dd6458f56ab94fee1dc01ef42fb8958aa134810e4d8fe1dd' +
      '4feee6af04742f80da5793875a78a2a4cc08d4e0a68ab03f1c022a0e8a7d3096089' +
      '92d24ecdd7e8f1895e3e5cd36e49906b531932d9ff958618b1a50f98455f515e0c6' +
      '3103d2e4e1651afc566eb9cad1e7efae1a9750c3880', 'hex'),
    'sha-384, no salt': Buffer.from(
      '7b95aab6b34c0962d228409e30df9b043c1b0baada08e73d887422552b8f1522e2e' +
      '42bf2b9ff2c6c9aa3eb0cd2370618e8f1a36873595e00bde75a9ce062ec32b5f639' +
      '4f2267a3f5c11840ff92e6e15bf31cc53e917ca8efc0895fb112c2ef8f681cbb6a4' +
      '10152f6e930caff1f260e31f983542e68cd15dea17ed3139cac735106fb05fc163b' +
      '2ed05a0ded939059a10c5cd7619e21b2d206907994274b34a4daefa1ce59b6b319f' +
      '73955a0918a5e237e1bbfdadb45c907a50083577e7192818845995b4a6d3ff1978e' +
      '0f9a42695853282e35c3b78133b3e0c624125aff14a1873d198f6304ffec7fc1cf2' +
      'adecc6cd14b1f89b1a637f72ed1ff5de7c6b4d96599', 'hex'),
    'sha-512, no salt': Buffer.from(
      'af1bc07fa70add19f3ce1f1bef8dfc6e24af43671cfb97e6b869e86b7ef03550a65' +
      '81318fff6449afa8b67e73e2a6a14e20677d8b067145a84422574ae0cfd2a5dff70' +
      'c6d7e97f6a0e166505079eb4264a43c493f2eb3fb06facc01be60774c277646a280' +
      '81247679622b220227e9249754867aa8fe1804015c4f98700982eda40e84d0ba033' +
      '6cf44f582fb8781374804e8fb43eb9d577acf4723587a39a2b4a9e168b767632b7a' +
      '554f77bc5272821c938c0994b162f7482636f7ffac564a19bd733f4877801dc324d' +
      'c47196ef12ca9a8f4921a5496cd6737935ca555b73466ddd817eaff03feda0eb2d6' +
      '12e3cdb59b1989eeffdc18101d46e56b9ff5c91f95d', 'hex'),
    'sha-1, salted': Buffer.from(
      '1f608a71d1884cfe2183b49037aa8555b0139a8a1267a5c5b9cce20701f2ad4bbd5' +
      'b329740bff31accc34bf9afd1439a0536bb32b6d427d26968dbc9e9c80d2111d948' +
      'c481cb1731778acd3110463241c4f23b3e13b855d162cb153851290fd95f781519e' +
      '2cef93745a413cfeec8e94fba7822b725d4744318458cf6b4a917b65b15ee6f54b9' +
      'c391f6064a9e031f7009f592449c0b46d5457a2799cb0ebd78a102a055ee0470b26' +
      '0c2b3d8ffbdee0fd47644822090ec55ae6233be1062f441c432ed3c275e74d62013' +
      '2681ec2e801e9b5b6acc1ad71f8935388f7e2c03370d12e944e3418c2ab63bb42ab' +
      'e1bb9e69530f02458ba28400b36806ff78da5791ace', 'hex'),
    'sha-256, salted': Buffer.from(
      '8c3d03bde8c42d9453631b0baac89e6296da20543713c004df35bc1a6fae205ab2b' +
      'f585369689073cdee345ad6e2783b2dda187b4979ea0457463758156e103eedd0ef' +
      '1834d35bd6ad540d9b8b225fd1770e514ea0af35f707f2e7a0382be6f5ed9d6b591' +
      'd536ce1215b17ef3eeb450bb48a0017497c67be0240470addd2891a81a8f1cf6e80' +
      'e3f837fe42376292df555b8b05931b69530597fae36dcd01b1c81767d4ecd4caf06' +
      'befc035224bdd2a5e6b89d51539235ac95570e757dbd70fdc15040001b07b937bf0' +
      '148ccc005f4c272acf5f8fc096a37d26208e96ac341c2d1d212c44d6d5156c934f6' +
      '6ef42fdbac77a208681550b048b466e32c76c7a7b07', 'hex'),
    'sha-384, salted': Buffer.from(
      '79f7284bb4216de68429854edb4218ef78ad1740848567377315db8867a15733c70' +
      '42e8bf90762e673c90c0e2c58c6c5cef497568bd92a6d219612c4756c55fac45507' +
      'f81608bc2720da4eedd5b23e1f3c8740c6b4cd7e4cf0e04342b184c1110199e6508' +
      '0d73b985e611d66f8e97990816e4917badbb0425dd94383892e2aa96de4db0de093' +
      '6aee84d5482a3da31b27319f43830fc48703cc7d4eaedb20fd30323dbf3f22608db' +
      '51637d3b305b3197962658d80935c266d33ccfb297590621f4a967c7245e92b0158' +
      'c0dcea943e2ace719ebdb196a9bae7df3ed9cc62765e27b63571743e28a0538db08' +
      '25cad2539eb5de5e6a320a88b573ec1972c26401530', 'hex'),
    'sha-512, salted': Buffer.from(
      'b74f3099d80787118b1f9de79fc207893e0d2d75c4110f4b159b85ba07d63a0256f' +
      'c3cd0f66ce8d9a2e3cf7a3d5a7b9c0befac6638894a3e36ce75e649ee069dd8dd98' +
      'aa8b602474c98b14bb03492de551a9e8e77934ef9b684583934f218d9576be240b5' +
      'c4f362eaf5e0140c8ea92639085a6269653505dcfa004226db9f63277653a64a182' +
      '6e4babb17ab54dd8543dcf1ce809706d6816e6a75ff846a3d4c18d11bdeb1f31b10' +
      'd55a3795b6496319e6e751504d86a4e7bb6535b9f0415e815d8c789c5b1e387f2a8' +
      'c00fef6e327462cb7e525b8f945be5b17248e0e0a4d855d397e22d067ce4539373d' +
      'fba46d1799250afc70f535006cacd2766f5ddcf8f91', 'hex'),
    'sha3-256, no salt': Buffer.from(
      '98787732f107a5390abc9ba3c93c2a0e30f6f31c3c76d73afee951a04525897df94' +
      '67c7532ff1b5c12601369339edcac4654a173e61780a12a21b5f0500bf16e2445f9' +
      'f7e9adab1ea2bb7e901f615b514965047d53b12ff2ff19f94f320179946bbf1b19d' +
      '88248e4fba7f49dc3c5af14de7a892a7718bd5962db33aa2b529c49e75d8fe936de' +
      '45e1db225ed875486516cd7398b5ec19043cf6005e06ba2d60f807d34d4ced378ae' +
      'cef2b1f75f6ad52cdd674d944e48d484be2e5f799510f244d089eb3570a674b2585' +
      '0616f12641e7e3e38e36fba1eefbed32d7a4809a4b5b1e557582303ab419bc128a1' +
      '813857157985f075d5d89e6867b7864dac0369b2513', 'hex'),
    'sha3-384, no salt': Buffer.from(
      '1d1399da211efc709b2cea90ff65e7c49162d943cadb59c78186889f7645e5d08f7' +
      '490de3f7b65ee5664c140dd61334182ddf45bcdc844845e4d60c917bf00eb1321e7' +
      '46cd7fce971af5ceea60b272219ccda2328b89a11b228cd42bdcc4c7eb40f0b6333' +
      '5f7496931baf36c0d2497045687ad27bb156c20f7fdae3baa38d57e35918f328bdd' +
      '2de7e6b23d6c676a18342a082f7ea019021903f103ccb4a6fddb2d88c30a1284764' +
      'b68c04bfe452c3adc6c10066a915231b7b404727eb6201b4921eb96d9407de2b963' +
      '3879ceb71d759d9828d7b4d062f6ef100757d8328187caf57dfb859d1555345207c' +
      '1cce7905c3564c08fec78867a53d5a2cf84810e1ffa', 'hex'),
    'sha3-512, no salt': Buffer.from(
      'd2430dc87abeaa7d13f7cec8510f1a296e1c608f44b1696829c59a99e8eefe9b2ee' +
      '6ee8ad6fdc93c24fcba2f04d1da195924b6209717e1992c10ed9f4783478765fe34' +
      '3e761203bff9d326bb6dc2061b0a7554c8ce0814b29249136c20c8e30054df0c6bc' +
      '656509a82845149368896690e32ff5dd32ef01543686f01d6a69bb438b049e66a8b' +
      'df485a13edcd7dc482da4cc57d0b740aca3e56f0da247794e600afd27b22b6da13b' +
      'cc15dd2059b525f8cb6bcd07540aa843f0ae51d4b0eea27045485914b908bdd01d0' +
      'a9d42379f9f7180f4ad162ff73df5fed0200eb02ad01473975d54a77c15a9c61a3c' +
      'b5e27de5d1eecc363d45506f7123a5ddd115c5e4c9e', 'hex'),
    'sha3-256, salted': Buffer.from(
      '59cb9cce6ae838eb20d38d6af4acb9b866b0753bb7df9e441037d788512c03279e8' +
      '3d9a9cf5c0921fe1c0b6e8e895a8c0ad24a18b123f809b34ef2a3a1f05974030320' +
      '435692ef5d378cef4368c3658c098a25371dfaf1c0b6910f653a4ec15f2c08956c1' +
      '405136c2aba7f25a808fa7dbf57a4cb2978bd91af710b27ee239d955c8cac7a76ae' +
      '9085cefeda2a585a99cc948f064b5da66a9c4aa4f3f767ac905a9f314b47038e05c' +
      '3608fbb7e67a278e4f009a62c3cd3fdf43692e759d9361be1217999a76a69d4d119' +
      'f8791a90e207e46b3f6125721f56fd819292d06a3cdae2c62c9a1dc0d964a06036c' +
      '8c18661cc6c873532a3536ab51e1ce210926db299e2', 'hex'),
    'sha3-384, salted': Buffer.from(
      '8d1f9297c8169f27f0c58827dba991d862de58c1155f612ad2995d2bf862d051c4a' +
      '91b48571849b0412384382e5b77990de6a3c84010046b35c4a504f175a3479483d9' +
      '5c58f86bb96d53a27e59d6f67fddaae295ce90610f5086acc711557c2c85aac32d3' +
      '24199cff2367ae44e1d91307a98c8cbfb085a8bce6b1c20714711bc15b0eddb7881' +
      '823227d4be477ffdad8093663a6a1fc62eb39c49c2c3a821c2b202cf7904b49ca92' +
      '3c83819602bb13931577354a80f99309030044935b1cd41f0513160e661db1959fb' +
      '1ec15f087f3d288e875d54cbf070ec860b0aeecc951ea65e97cd5460750d4b7de52' +
      '22cb9e7466b1f506ecf6a81fc399dfd8334160f9084', 'hex'),
    'sha3-512, salted': Buffer.from(
      '7b6d7be418c5d37cc8070698b8b03d818ecd8b673d047d34921913f6d59c69cb496' +
      '172d6118207d9ff92b8e1246acf0e03a845d935a70f8a82c3d5d6db6a1a0e337269' +
      '4b904372413dcbaa7ac5486bc8ccaf70d7e9470be82b928a90017e272cf9761ed26' +
      'c160fe874a2675a4fb2acad72c50fbfffdd70b5a6f2919553d7ea1829934670f8de' +
      'f2a5c2816404b1aa153323c92c58400622f184b9b0463fa48d6b27091f68c287e3f' +
      '6d9ab9eb451711a5d984c547f3d56f14a686a89ddf36c47ce25092b8c6530904de9' +
      '5df7fc602fe9394315f1b1847aae304cb5ad71e2cb78acfbc997a87a9d62a6898bb' +
      '6d84a81bb89b50186265f4be171a93d837a4bf777c8', 'hex')
  }

  const vectors = [
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 0 },
      hash: 'SHA-1',
      plaintext,
      signature: signatures['sha-1, no salt']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 0 },
      hash: 'SHA-256',
      plaintext,
      signature: signatures['sha-256, no salt']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 0 },
      hash: 'SHA-384',
      plaintext,
      signature: signatures['sha-384, no salt']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 0 },
      hash: 'SHA-512',
      plaintext,
      signature: signatures['sha-512, no salt']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 20 },
      hash: 'SHA-1',
      plaintext,
      signature: signatures['sha-1, salted']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 32 },
      hash: 'SHA-256',
      plaintext,
      signature: signatures['sha-256, salted']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 48 },
      hash: 'SHA-384',
      plaintext,
      signature: signatures['sha-384, salted']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 64 },
      hash: 'SHA-512',
      plaintext,
      signature: signatures['sha-512, salted']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 0 },
      hash: 'SHA3-256',
      plaintext,
      signature: signatures['sha3-256, no salt']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 0 },
      hash: 'SHA3-384',
      plaintext,
      signature: signatures['sha3-384, no salt']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 0 },
      hash: 'SHA3-512',
      plaintext,
      signature: signatures['sha3-512, no salt']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 32 },
      hash: 'SHA3-256',
      plaintext,
      signature: signatures['sha3-256, salted']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 48 },
      hash: 'SHA3-384',
      plaintext,
      signature: signatures['sha3-384, salted']
    },
    {
      publicKeyBuffer: spki,
      privateKeyBuffer: pkcs8,
      algorithm: { name: 'RSA-PSS', saltLength: 64 },
      hash: 'SHA3-512',
      plaintext,
      signature: signatures['sha3-512, salted']
    }
  ];

  return vectors;
};
