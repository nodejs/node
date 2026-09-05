'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasFIPS, hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 5))
  common.skip('requires OpenSSL >= 3.5');

const assert = require('assert');
const { once } = require('events');
const { KeyObject } = require('crypto');
const { MessageChannel } = require('worker_threads');
const { subtle } = globalThis.crypto;
const { SubtleCrypto } = globalThis;

if (hasFIPS()) {
  (async () => {
    for (const name of [
      'MLKEM768-P256',
      'MLKEM768-X25519',
      'MLKEM1024-P384',
    ]) {
      for (const operation of [
        'generateKey',
        'importKey',
        'exportKey',
        'getPublicKey',
        'encapsulateBits',
        'decapsulateBits',
      ]) {
        assert.strictEqual(SubtleCrypto.supports(operation, name), false);
      }
      assert.strictEqual(
        SubtleCrypto.supports('encapsulateKey', name, 'HKDF'),
        false);
      assert.strictEqual(
        SubtleCrypto.supports('decapsulateKey', name, 'HKDF'),
        false);
      await assert.rejects(
        subtle.generateKey(
          name,
          true,
          ['encapsulateBits', 'decapsulateBits']),
        { name: 'NotSupportedError', message: 'Unrecognized algorithm name' });
    }
  })().then(common.mustCall());
  return;
}

const vectors = [
  {
    name: 'MLKEM768-P256',
    seed: '0000000000000000000000000000000000000000000000000000000000000000',
    publicKey: '3d209f716752f6408e7f89bceef97ac388530045377927644ef046c0a7cae978c8841a0133aa' +
               'c4f1e1a7027277f671219cf58b85d29c8fec08edd432e787a3cf9936fe0026a113cb9efb1d72' +
               '14049527bfe2141ea170b0294a59403ab0ce16760a8baa95b823cbb8aacdcc17ef32775223c7' +
               '91e3740163941f9bb3f63346bef1c050c31f932c62719429aff14c2bd438ab135bed692d56c7' +
               '7c04cbbffd6335b578318b513771e84b14ea821262141ca006ccb8bf2500aa1008970f216fe7' +
               'f1ae34125aa290492c069a189222adc322f97649c762c7d3128ad3bb2667971d0744014bc3b6' +
               '7445cbcd0b3e7ea69fb1cb9f9c331f97487920187292926d04a25a2650abbd44982bb0c3c630' +
               '1fe6a61330d24d8a3c7021dc3e3392c79a139b37613bba67a2984298507b84a4d61eef18acfb' +
               '979af2d39caa4c0db4513815359d76fc378c63a7f4f3053b17168d0221cf0c2eec5514ba235f' +
               '81d04d67c3b5c518094917671c26a7c046457533cc32844581277a03eb065c4529a779a9a587' +
               '8f2aac3f81db9ed3d8c9345697058cbb99d379bca16d8fdb61d129960390524791b9d3e501b9' +
               '00bd1e5002e095be06c23f1fb212f5801f24b6b28c0c5493d246d02aa29fa3acfbe15ac4e212' +
               'eb0b6f69ebbea259a2703aa4c308224bdb741c65c7a5d4bff788279507bbfe513d7aa5694e7b' +
               '3cdf62ab36432742d4a0ca9b3570ba742fa803b46989c8526ea586cc4fc32866143b79601725' +
               'fa545fd280b404530318bbc3371194710b6d74beaa629eb18a36a953b75915ae96999ba5c88c' +
               'dc56a46861c50032c9b630bcc1445a30878979bc55a2c0955bf399b231203b90c651b6afe0e2' +
               '42b5a543250b142f7291ed753d816098f7913302a8ce91641716623d4fc2ac6772aa5f367404' +
               '2b7c4a18a2186289a4ac4e200774596ca03e6798c7506b984999db6ac142586bae0799f1e776' +
               'f9f5247dc574d8556ddf9bbbc4ca3643263457f74248010d62d4311268360aecb4902b450bf2' +
               '050ecb8ba7a92820d233f5a14ed31225a1d17ca6f19e825894cfb1807d922cbd60761134be41' +
               '9144bcf72006366a4460137ad9136c113f05eb54c409520edc72e4150cc3a24b0f819eec11bb' +
               'd19ca9645b0810a60b4a8a9e9c3955396a1653955b047bcf4f98433c27236c570d75f809e44a' +
               'af2dc33665826351872c293350ab324518c8c0c80b521c80c81a56bdc968a5650315a830c8bb' +
               '17532c62ccc23b1d46412c256b224fd4674491803501d0143125c7577239689965b6989ca561' +
               '793c0f85c62a9e13487da17662a7188c70b1040a67ed4c3f85e74e3691822fb96314d6134fe6' +
               'a626b3cbe1461d62a7b573b2cc75579ffa22967e36ceb2a1aa0b71875a22751d706b72ca9ecd' +
               '0c8100ad0aa58009a5c83fffe91759e6baa0a9345af99fe3b69509dbc84032868844ab3f65bb' +
               '1df8beadf36442e48e339c967023a525411544c789a2f04dacd06ffef78302210450b931f6b4' +
               'c32aab34a3f5260b810f4c9a946fc22d3baabaa80ba8d9955d6dc35e8609b4256b482cdc9d89' +
               '77c1a47a354e7c527fdb1672e166917b95cd6351820261daab361f8a2dcbb240c55abd6a8105' +
               'e5291b427b566d731e6b7047189cff20d8b120e0b3e72472d1b0086812200fd3698e23f06e4f' +
               '4e08bbb54cc2049c039c845be659999c8fa48d7f62327c146cf1bc0b0bb1b91b30174b7bc220' +
               'd422023bff6b0dee263532c503f3982e4d3e27071b855578a9a9aa63b8a8c339bf',
    ciphertext: 'd81018a94f8078e02105beaa814e003390befa4589bb614f77397af42d8e8150796f2c88a4ef' +
                'ca81b8cf93c0ae3716c54ec1b045e3875f38c2dd12d7f717bd7fb701a9fecda5ed8b764c9a35' +
                'd4a5c1d8930f6071f653eebb2d1afa77debb8302d16f17e0f5f3920a71a4d49beafa0e1c7e44' +
                '3f8abca64a65a9e81a97e7357bf902573363c0e1a12e5228036828e3f759121fada92441fe33' +
                '4e85d79347e470d2fed945541d832c54baaa3cb7526c3853954db4f73547cc7c27fd38398bfa' +
                '7704952cb841e38b270e4db7435f0ee22f57d7ad3270bd0c88e71b4b864cf2277c65daa10a6d' +
                'ad4c7abecd95cc4ebec39c08404b522e4ecc1545713f76bebd3b5a0f2feb3461936065dbd13f' +
                '6a1f61e1b142a2af2e5a482ba2c50cf0317049c0b3bfd6d5e9240eba9111d2030fdea17e33b6' +
                '524020d30b0c4f8069285f3a6ca267d287d01e827d8422bf5426e11688bfc73756af1841b1c8' +
                '7e126cb50c914b5b2b8673488ad3b074cad77a3840eb12dd688f313ee1e9ff8c479a678f2763' +
                '56fc9d65e1d5b4c1e9855b4175db144f7767c12061769190fe6b5e51563b91f94d131a2b796b' +
                'd2980ed0dab4ae7a7110e920007a757158a5eb8662cbf89ddffe9d8196821313cdc00108853f' +
                'c4746b111d5b56da638d8ed2973918960f5dfe93ead3ae521e957cec3c8d843e8fce234c70ad' +
                '055177f235439d6098bdd771b1cfcfadaab4f50a7378185c62409f383c8ff658c2a2af66498c' +
                'fd81e962766ac6b774e88424fb4f331837d0a28502708477caf8780a156d723f68fca791e1cd' +
                '2397bfc2b24c77c765d9b2af36f732d52107517efd8157b283b440a613f756c364ca108971a8' +
                '878199a93f260baec3e850033cc032c2e53f823576affb4d3b116e2d16049152c35aaa263ab3' +
                '76f0ad5ede6a749607a283e3016e62191c0e8fde33e718cd989591c9a205d608d99fcb8a7471' +
                '603d716cb01b56328d7d880aec2851f4e6d8b5016c25647e9026ebb441543e8012dbfcf078d4' +
                '012b8c39184dd64f3821b4774ae4e36365f8baf2bd1f6667c017a1e65ff8a1554458fb3f367c' +
                '02721752bfa56fc7fd566ae95ffb208f919ef12f4cf8a2fdd141a8df559bddb7b8d1f04ee6d4' +
                'cf7805d142989caf216dfae985faaab9974f6d9f8aa1129084db8db912b1655f595ffbaa6649' +
                '1ab4655fd734cfd4bb0c0289d4bcc8fc5e9943b351cb147c8db059a24004d1c3e3bb4c14a881' +
                'e5101acb736c65c5d579acb67ee85a560277b43338fe79d34b772c5da001da3b5a3383dd8131' +
                '9a0b4542e6d7e46eed5314cc70eb231de27b6e760db598ba19995cf69be0e4458e35f3f274ac' +
                'a2455d43fe3344e183c6dc47c857dbe9907b41e41006d91b25adcafc098fe66f7554be8dad49' +
                '3c4f4b1dbf7a51464139db474afab5572f92a2232b59be56a72c0505149dae5cde1e60287703' +
                '7de7802b5f6fa47a4c9a3e52d6ca15339920254e9ffb53c7b834cc0288ed9905a1841e9390ea' +
                '94a8898bd4c6b6d6027e4d43c7867242515bbeefe12340fc04428a824ea7cf56ad2a64ed368b' +
                '71315d80cee846007cff1d2eea2c3f0f921537304ae598f98dd10d1f102811a4e2d161c3fd8b' +
                'bb193d4b25bee950ac839c0f9d',
    sharedSecret: '9bd018e869bb01b63fb8f5da374a73d347ea14cb2bc570b13d0908e2288ec456',
  },
  {
    name: 'MLKEM768-X25519',
    seed: '0000000000000000000000000000000000000000000000000000000000000000',
    publicKey: '3d209f716752f6408e7f89bceef97ac388530045377927644ef046c0a7cae978c8841a0133aa' +
               'c4f1e1a7027277f671219cf58b85d29c8fec08edd432e787a3cf9936fe0026a113cb9efb1d72' +
               '14049527bfe2141ea170b0294a59403ab0ce16760a8baa95b823cbb8aacdcc17ef32775223c7' +
               '91e3740163941f9bb3f63346bef1c050c31f932c62719429aff14c2bd438ab135bed692d56c7' +
               '7c04cbbffd6335b578318b513771e84b14ea821262141ca006ccb8bf2500aa1008970f216fe7' +
               'f1ae34125aa290492c069a189222adc322f97649c762c7d3128ad3bb2667971d0744014bc3b6' +
               '7445cbcd0b3e7ea69fb1cb9f9c331f97487920187292926d04a25a2650abbd44982bb0c3c630' +
               '1fe6a61330d24d8a3c7021dc3e3392c79a139b37613bba67a2984298507b84a4d61eef18acfb' +
               '979af2d39caa4c0db4513815359d76fc378c63a7f4f3053b17168d0221cf0c2eec5514ba235f' +
               '81d04d67c3b5c518094917671c26a7c046457533cc32844581277a03eb065c4529a779a9a587' +
               '8f2aac3f81db9ed3d8c9345697058cbb99d379bca16d8fdb61d129960390524791b9d3e501b9' +
               '00bd1e5002e095be06c23f1fb212f5801f24b6b28c0c5493d246d02aa29fa3acfbe15ac4e212' +
               'eb0b6f69ebbea259a2703aa4c308224bdb741c65c7a5d4bff788279507bbfe513d7aa5694e7b' +
               '3cdf62ab36432742d4a0ca9b3570ba742fa803b46989c8526ea586cc4fc32866143b79601725' +
               'fa545fd280b404530318bbc3371194710b6d74beaa629eb18a36a953b75915ae96999ba5c88c' +
               'dc56a46861c50032c9b630bcc1445a30878979bc55a2c0955bf399b231203b90c651b6afe0e2' +
               '42b5a543250b142f7291ed753d816098f7913302a8ce91641716623d4fc2ac6772aa5f367404' +
               '2b7c4a18a2186289a4ac4e200774596ca03e6798c7506b984999db6ac142586bae0799f1e776' +
               'f9f5247dc574d8556ddf9bbbc4ca3643263457f74248010d62d4311268360aecb4902b450bf2' +
               '050ecb8ba7a92820d233f5a14ed31225a1d17ca6f19e825894cfb1807d922cbd60761134be41' +
               '9144bcf72006366a4460137ad9136c113f05eb54c409520edc72e4150cc3a24b0f819eec11bb' +
               'd19ca9645b0810a60b4a8a9e9c3955396a1653955b047bcf4f98433c27236c570d75f809e44a' +
               'af2dc33665826351872c293350ab324518c8c0c80b521c80c81a56bdc968a5650315a830c8bb' +
               '17532c62ccc23b1d46412c256b224fd4674491803501d0143125c7577239689965b6989ca561' +
               '793c0f85c62a9e13487da17662a7188c70b1040a67ed4c3f85e74e3691822fb96314d6134fe6' +
               'a626b3cbe1461d62a7b573b2cc75579ffa22967e36ceb2a1aa0b71875a22751d706b72ca9ecd' +
               '0c8100ad0aa58009a5c83fffe91759e6baa0a9345af99fe3b69509dbc84032868844ab3f65bb' +
               '1df8beadf36442e48e339c967023a525411544c789a2f04dacd06ffef78302210450b931f6b4' +
               'c32aab34a3f5260b810f4c9a946fc22d3baabaa80ba8d9955d6dc35e8609b4256b482cdc9d89' +
               '77c1a47a354e7c527fdb1672e166917b95cd6351820261daab361f8a2dcbb240c55abd6a8105' +
               'e5291b427b566d731e6b7047189cff20d8b120e0b3e72472d1b0086812200fd3698e23f06e4f' +
               '4e08bbb54cc2f63601b7f85accfeea2d17964c66b5194b0f08e18519faaee194e3c102823062',
    ciphertext: 'd81018a94f8078e02105beaa814e003390befa4589bb614f77397af42d8e8150796f2c88a4ef' +
                'ca81b8cf93c0ae3716c54ec1b045e3875f38c2dd12d7f717bd7fb701a9fecda5ed8b764c9a35' +
                'd4a5c1d8930f6071f653eebb2d1afa77debb8302d16f17e0f5f3920a71a4d49beafa0e1c7e44' +
                '3f8abca64a65a9e81a97e7357bf902573363c0e1a12e5228036828e3f759121fada92441fe33' +
                '4e85d79347e470d2fed945541d832c54baaa3cb7526c3853954db4f73547cc7c27fd38398bfa' +
                '7704952cb841e38b270e4db7435f0ee22f57d7ad3270bd0c88e71b4b864cf2277c65daa10a6d' +
                'ad4c7abecd95cc4ebec39c08404b522e4ecc1545713f76bebd3b5a0f2feb3461936065dbd13f' +
                '6a1f61e1b142a2af2e5a482ba2c50cf0317049c0b3bfd6d5e9240eba9111d2030fdea17e33b6' +
                '524020d30b0c4f8069285f3a6ca267d287d01e827d8422bf5426e11688bfc73756af1841b1c8' +
                '7e126cb50c914b5b2b8673488ad3b074cad77a3840eb12dd688f313ee1e9ff8c479a678f2763' +
                '56fc9d65e1d5b4c1e9855b4175db144f7767c12061769190fe6b5e51563b91f94d131a2b796b' +
                'd2980ed0dab4ae7a7110e920007a757158a5eb8662cbf89ddffe9d8196821313cdc00108853f' +
                'c4746b111d5b56da638d8ed2973918960f5dfe93ead3ae521e957cec3c8d843e8fce234c70ad' +
                '055177f235439d6098bdd771b1cfcfadaab4f50a7378185c62409f383c8ff658c2a2af66498c' +
                'fd81e962766ac6b774e88424fb4f331837d0a28502708477caf8780a156d723f68fca791e1cd' +
                '2397bfc2b24c77c765d9b2af36f732d52107517efd8157b283b440a613f756c364ca108971a8' +
                '878199a93f260baec3e850033cc032c2e53f823576affb4d3b116e2d16049152c35aaa263ab3' +
                '76f0ad5ede6a749607a283e3016e62191c0e8fde33e718cd989591c9a205d608d99fcb8a7471' +
                '603d716cb01b56328d7d880aec2851f4e6d8b5016c25647e9026ebb441543e8012dbfcf078d4' +
                '012b8c39184dd64f3821b4774ae4e36365f8baf2bd1f6667c017a1e65ff8a1554458fb3f367c' +
                '02721752bfa56fc7fd566ae95ffb208f919ef12f4cf8a2fdd141a8df559bddb7b8d1f04ee6d4' +
                'cf7805d142989caf216dfae985faaab9974f6d9f8aa1129084db8db912b1655f595ffbaa6649' +
                '1ab4655fd734cfd4bb0c0289d4bcc8fc5e9943b351cb147c8db059a24004d1c3e3bb4c14a881' +
                'e5101acb736c65c5d579acb67ee85a560277b43338fe79d34b772c5da001da3b5a3383dd8131' +
                '9a0b4542e6d7e46eed5314cc70eb231de27b6e760db598ba19995cf69be0e4458e35f3f274ac' +
                'a2455d43fe3344e183c6dc47c857dbe9907b41e41006d91b25adcafc098fe66f7554be8dad49' +
                '3c4f4b1dbf7a51464139db474afab5572f92a2232b59be56a72c0505149dae5cde1e60287703' +
                '7de7802b5f6fa47a4c9a3e52d6ca15339920254e9ffb53c7b834cc0288ed9905a1841e9390ea' +
                '94a8898bd4c6b6d6027e4d43c7867242515bbeefe12340fc6b3d57762f8badb69433f9c6d060' +
                'f85f5e5c6b6803a816d141c075f63541ad10',
    sharedSecret: 'e5ba94031ea6efd69c09c254f6d9783136ba6037e2d4c43bcccf19d6f3f4343a',
  },
  {
    name: 'MLKEM1024-P384',
    seed: '0000000000000000000000000000000000000000000000000000000000000000',
    publicKey: 'a10bc8b554cd51980cdbbccc3041420fd320fe8b74c7a84278c63c17070dc231b61ab269b9d6' +
               '77d920261186654b4571f51797d5c342b8070bc6c92bca16adecc631e4e94c7508b111730c74' +
               '9c73e2d6a6f97155cb269ccc06a71a21bef3d269463c935048a7f4636c7b320073709023f7b0' +
               '4d0530571a9a6f718280870bb63875d3f599bc229b95869cd5bb5d26640856d40b828198fdf2' +
               'c099998ffdf772e462336c521cd326b5e4997bd95c135c57bd02c7afa80a2923d510951778ee' +
               '5125b2aa18f90445453b85789224725b259279698ac9426c882baabc38d4fb3a3f6831180918' +
               'b9825e0e418154d78aebab5e7e7066e69b2567476bf1177fe079a38298be6f01b098c33851ab' +
               '25312b52e32a5750c2b73d293c0b810473b310aaf062f19914c7377b2e90388f575bf5e68534' +
               '53b95a74aa18d62d4ae37e6996a48ab5217488a92d7b01e315c50b68204143792afc4f8367c0' +
               'ce065ab32014bdb5515fe0594608aad1218994724afaaaa2df0355f46666b6e02a387b6d3da4' +
               '713edb610bb048c3a2078b800e9ea483f2009c96d24c71b2cbc8e1200c0277383c5c27895e29' +
               '8c3607701ce58702a91903274a041408234cb0021ef2b1c5131419b444dc84b89d147d1fe43c' +
               '43f676d906735d9ca2a59c2232d97fd4aa1ae2bb3d1b170ca553cb2574954fdc6689fac623cb' +
               'aa31982d82424d5a564fef7a8ba51b44df15053b2b45bec4aa1ed49929123daf754175c59382' +
               '58c608b24d062042ab4bbee5e553a5ea627521738ae5ab2e06bd98b020787b2f5fa51eb4c46c' +
               '2bf90e55a49560340667f88ac41432b7f551dfd98c037c79f79b41b985a8b1f51345550cd816' +
               '714362040778c43e378a288394bd028c8c31b5a904bc4a5648a596035cb38f0e276e12c9a96f' +
               '8425056b05a136642dd2cb75463036485ba1a50539e420e1e31dfac529cad6c68ec067467494' +
               '73e050a4ac92b7199beceb239b6c12c8e716b66607aeca64a5850b01f99d0b176a7759781ed7' +
               '7cb1ba40d17ac5c6cb06c942c002c2cf6efcb121f10ad2a45ff781426e7104cbdca73b81865a' +
               'b22b00ba834355ae485a262f354248932c2be178369a3dd7e2428fdc379346ab2b754c43db65' +
               '7460cb09c5c48b5810cb7a5c6156cf87440c9e36a4869a8ac458b382fc178915a9ce1bcdda7c' +
               '48807c207e656ffb80bf33e32bc8c7b20ef60572612ceac99ad1c56ce5a764b29b74c17a5b51' +
               '0b1afcb18a1afc35c12ac213725325f9b7a2eb338fe4c0080c31a58a995db7027d900e785448' +
               '87f90ada467d0e383c119c5399310bc6735874e8804ff6c2bae57f2c3357cb627033c12a5924' +
               'b20ce5abf113172bd2b77086cac543811793bba71734c9f005ac2656460bc30a442b38872575' +
               '8a623e37ba6e293abfb84f344229f373c214ca776a7c05adc465fed93b9cf77f0022ab71f1ad' +
               'de369dd8f420a58c057c14cc18dc47da7c12b086473eab419652967001c4e42a381c8ba539a8' +
               '75d21a9945133bab9bc1e53a600de77cbfb2aeab6b19ced4c6eaa8998ee6a1577255f7132d80' +
               'a32d6c0c6ec44c9c4b28699a645bb0bc958e00275077925309519b0824c7000dfa61912ec049' +
               '063a067d00b059053e508a5bfee63473869c8a8510af898cd7572854f5c38af96f5f97a73726' +
               '32ea7bb4b6fb831c612af71191ff9806b379bcd43c6059b7b1f953741444af713c155d962722' +
               'b947aa23a32a89b356a6a7508aad63968c1dea78ff18aac27a89aa7b42b0d7481dd3cc649421' +
               'e51397782218ac5441760ba51a0328d66b436fec32d7aa4d68e0cad1bc14f7241c903480f809' +
               '983fc2c30d93138cf63b59bc737ac08192893d039187a811bef3d3209eb7b8d1e05b5b251cef' +
               '760a210b2732867ab32049ba3c354e3858aee7b71df792924730d8e842e484122b50677b0a30' +
               '6e61cf21b62091da18b937192936a09e5a418cf78b666157dd477af1c36a12320129522840e3' +
               '70941157808782a5335b0ac10d70e1beafd401074b84b9826cc58aad217bae0f419b2da89613' +
               '3272d8f22c6f420fcc738fccc1082fc93c7df0994c6bcf2cc8a29037b6bb2b4bcef4b0ee8caf' +
               '8506bc5ecba082a56806c1cede0b944338a69a668254c1150ae05030e256b2b67661ba027d97' +
               '576da613ac8c7c29051f1240b96b0c127e264d5e1dbbfe9561a567d5c9103673b446b3ccea6c' +
               '5f7f34f09348a5d4a58b0498871dc940ee97b50c0336f9a60c3299f99560ac70657a27befa70' +
               '2265ce590583e04a28326092d3dea2118dd1df5e81d7d3014ec4b5ce67dcb45ef001769dd5d5' +
               'ada76934d38d740924712bfae672169d8f8744c151346d285fbb653f83aa0f',
    ciphertext: 'dc63d18bb9715fb6e3ba71cb439fcd3377a75305cc9b144e6758bf5794a272e6b4a0da33234c' +
                '0ac1bb5b4e60e4c82eb1fb780d59e4e4616641a0595ba031e3ae69d971dcd5fff14e21731a8e' +
                '1a221f46c7820d214630b707fa1b0de3a484698f3d49e0a75f1212b8c42d330dd909f15eac04' +
                '02f19ee77fba9447e1c44304b0d8c371c17c5549fdbdec1e0a2e7be9f577d7a4b5b2618d9ba6' +
                '7ab95a0297cd5c5a13c89cc5a57cbd9a8ae38d66455c9a3d2bc55b498775fee2f6dc224d376d' +
                '5f526a8354c8ed724f60337e900b85627972383e1fd987d407a8834005814a4fdc94c947e5f3' +
                '471459288cfb127952b3208f10c914200bbaac5fcebd2bc9e2848492bab17b9288ca8b81d1c2' +
                'ac9522dcc0b6d5f51e10f3afbb5d65fbf919edef6323c4e92c6b0690c10db25a9182de9e919e' +
                'a1b3e65ae6150635d5180ebd7d23a2264828bc3ee1fd34dba1924ad0db30c747e05baa9148f1' +
                'a032769c685e04665fd802a79c4624f69a9198a426eac1b217d903cdacf8844e73365f3a219a' +
                '700dda27edf6bea33602617c5fd105b301b884bfaaa1163b791ec09f82523fef65c87b75ed06' +
                '3ceb127729b82c8712e1f41b547d095f55ee71f3f8b47a306cb5d9bdd817854c74a42eebf934' +
                'a1136dea3fbc546ad8ce51b3171913722f08b0261d197590342bfe4108dcb08c62a98610cbfb' +
                '8d3b2831f56dcac2220e29a5811f38f0824f21a6cbebc64fd89a09b110dffbe03799ffc74fe5' +
                '65c80dbf6a66acd7bfd14cb90acba03405a7982d4c1c68caa75f8b72e4dd6401d7dce4db4f6b' +
                '820a7886a604b66b4e5b9eea5e5eddc2bca458a25977bd1f02874c5d9daf2baf56b3040f24ce' +
                '7fe14cc14d61c7960db4decb37d9779c8e36d69a7763066d8c1149312d26887a693dc222daa8' +
                '92dd00cd8f3a558cf605e4c65c011c2e9f0d671ba10af2bb90ee0351ae5078eb7878399ec9eb' +
                '4ace87a68269618bda12a7aed6fda0385496c5d10ac36b35255f4a31edfa8a2c516b65c63431' +
                '013ed4909ec7a787a5efb9d3c3887b80ac18a44934b6559bd8a84b18e86fa1b0b9e1d9f92ba4' +
                '95ba5595d82e5095612b79e805154bf428a7071662c7cefb6450165c6f8f6954c37219bff4a4' +
                '9894a8aa37f940a40f4ec942c281e6c47ea408199927a724ff1c7460fc8fd47a98d0c9d4d1f0' +
                '7994d8084f6e084935ad7c2985282fabd5ca13b942e10d35278f4ff4cb1cb96f3c862410e791' +
                '44a46b4db1a3c3d4d63018ec5c01ca48cb67081482e7d434b4abe5fa3071f2fbb533f745602b' +
                '0da6183b28e6c5dfa42dab7ae0bbbf7638e106be1bd7312cba399e08c96dbd69a128a2face2d' +
                '4a02951533a25e82fe63d0aaaa2e8c75150215c93ab06c22f9cab8d1cae7424f8baa09b3260e' +
                'cfa3c7c8d55a276b4b317f72ec86b1b145a63aca83ef8c1204d8ab0c96ea3f742de39db47020' +
                '616e139285814f188029ace4587f14cf12b5ed81086d8213cf8cb578341e04e16f519b77ff4c' +
                '2644a5732639d658d0c4eaf992bd7dbd5011b700a5fa63dc1b24a84a3c80656bab5705dc3a74' +
                '312c80e8bdb24a7ac6e27bcb8c07ece62c6e5777dd3dc0657181f440c7524d907dd27950bcb2' +
                '52aef7f8cbf453cee3fe3143a665072c787cea76de323aa41537df2f3a40a518a694b918953b' +
                'de8d57084e32d3b1fdcf9d153e73f02624beaf6ebe23e6828a6a489583494f3cd790fc96bb6f' +
                '5d8b198402965e2e668e6581e7cf1c8a47a92198388f2b4cd38df660f0ddd48ad126819c4435' +
                'af3a12c89113d778ac544fd8079cb8aaa97d2ff1b608da574c4dcd87f4979390de3be405f0e4' +
                '7788dd0b01662805079fd73c64e9278c036544add3694c838bfcfb08c8a5efb09549442123ea' +
                'a59fa30fbb9198105f6be00163bac076193f6721c539714108bbfae167f5db8085c5838618f3' +
                '2a968bbb25c40645a17c17b9bec64aea45832eec5adc25b53e677f67566fbf5ce2d9193a06bd' +
                '9b477e601d589b25f422defc49105252cd9ca6adcbb36be8a01a8472b4d463f655be14ccff9b' +
                '0571a2048e31c14b9b23e2d43fafa3f85ece6fd41896cc5c68993dbaa926f285ec94c72887de' +
                '9564881d735c05f83aa474b3d4cd133a630ac63850771cb5270f6cb7a391170d66af3e4901b6' +
                'eb0253f3f34ef57d6babd97aa99ce718c3bcb53ff13d4028a0c943bb9681106ce176242cccb7' +
                '5df1d3f8d3706e5b068b042c3154d5e6292581b36499e6b069b9a490aa67f0675390539da855' +
                '5e6a4e8a35a86fdfea83e1387bf4acc650ec1edae7c99aa3a48306ee1d1a5e513c0c6901f64d' +
                '0a3ee285de3c11d49f90cd4323dafda14832f0d8b760c0e5a48633c967cfaf',
    sharedSecret: '8c028c6ea72a1c59408e2b15dd8fed8008517e861cd2329b159bda1919ea656c',
  },
];

const lengths = {
  'MLKEM768-P256': { publicKey: 1249, ciphertext: 1153, groupElement: 65 },
  'MLKEM768-X25519': { publicKey: 1216, ciphertext: 1120, groupElement: 32 },
  'MLKEM1024-P384': { publicKey: 1665, ciphertext: 1665, groupElement: 97 },
};

async function roundTripViaMessageChannel(key) {
  const { port1, port2 } = new MessageChannel();
  port1.postMessage(key);
  const [received] = await once(port2, 'message');
  port1.close();
  port2.close();
  return received;
}

function buffers(vector) {
  return {
    seed: Buffer.from(vector.seed, 'hex'),
    publicKey: Buffer.from(vector.publicKey, 'hex'),
    ciphertext: Buffer.from(vector.ciphertext, 'hex'),
    sharedSecret: Buffer.from(vector.sharedSecret, 'hex'),
  };
}

async function testAlgorithmConfigPrototypePollution() {
  const properties = [
    'encapsulationKeyLength',
    'ciphertextLength',
    'groupOrder',
    'namedCurve',
  ];
  const descriptors = new Map();

  for (const property of properties) {
    descriptors.set(
      property,
      Object.getOwnPropertyDescriptor(Object.prototype, property));
    Object.defineProperty(Object.prototype, property, {
      __proto__: null,
      configurable: true,
      get: common.mustNotCall(`Object.prototype.${property} getter`),
      set: common.mustNotCall(`Object.prototype.${property} setter`),
    });
  }

  try {
    await subtle.generateKey(
      'MLKEM768-X25519',
      true,
      ['encapsulateBits', 'decapsulateBits']);
  } finally {
    for (const property of properties) {
      const descriptor = descriptors.get(property);
      if (descriptor === undefined) {
        delete Object.prototype[property];
      } else {
        Object.defineProperty(Object.prototype, property, descriptor);
      }
    }
  }
}

async function testGeneratedRoundTrip(vector) {
  const algorithm = { name: vector.name };
  const { ciphertext } = buffers(vector);
  const keyPair = await subtle.generateKey(
    algorithm,
    true,
    ['encapsulateKey', 'encapsulateBits', 'decapsulateKey', 'decapsulateBits']);
  assert.strictEqual(Object.getPrototypeOf(keyPair), Object.prototype);
  assert.deepStrictEqual(Object.keys(keyPair), ['publicKey', 'privateKey']);
  const { privateKey, publicKey } = keyPair;

  assert.strictEqual(publicKey.type, 'public');
  assert.strictEqual(privateKey.type, 'private');
  assert.strictEqual(publicKey.algorithm.name, algorithm.name);
  assert.strictEqual(privateKey.algorithm.name, algorithm.name);
  assert.deepStrictEqual(publicKey.usages, ['encapsulateKey', 'encapsulateBits']);
  assert.deepStrictEqual(privateKey.usages, ['decapsulateKey', 'decapsulateBits']);

  for (const key of [publicKey, privateKey]) {
    assert.throws(() => KeyObject.from(key), {
      code: 'ERR_INVALID_ARG_VALUE',
      message: /cannot be represented by a single KeyObject/,
    });
  }

  const encapsulated = await subtle.encapsulateBits(algorithm, publicKey);
  assert.strictEqual(encapsulated.sharedKey.byteLength, 32);
  assert.strictEqual(encapsulated.ciphertext.byteLength, ciphertext.byteLength);

  const decapsulated = await subtle.decapsulateBits(
    algorithm,
    privateKey,
    encapsulated.ciphertext);
  assert(Buffer.from(decapsulated).equals(Buffer.from(encapsulated.sharedKey)));

  const encapsulatedKey = await subtle.encapsulateKey(
    algorithm,
    publicKey,
    { name: 'HMAC', hash: 'SHA-256' },
    true,
    ['sign']);
  const decapsulatedKey = await subtle.decapsulateKey(
    algorithm,
    privateKey,
    encapsulatedKey.ciphertext,
    { name: 'HMAC', hash: 'SHA-256' },
    true,
    ['sign']);
  assert(KeyObject.from(encapsulatedKey.sharedKey)
    .export()
    .equals(KeyObject.from(decapsulatedKey).export()));
}

async function testVectorRoundTrip(vector) {
  const algorithm = { name: vector.name };
  const { seed, publicKey, ciphertext, sharedSecret } = buffers(vector);
  assert.strictEqual(seed.byteLength, 32);
  assert.strictEqual(publicKey.byteLength, lengths[vector.name].publicKey);
  assert.strictEqual(ciphertext.byteLength, lengths[vector.name].ciphertext);
  assert.strictEqual(sharedSecret.byteLength, 32);

  const privateKey = await subtle.importKey(
    'raw-seed',
    seed,
    algorithm,
    true,
    ['decapsulateBits']);
  const publicKeyFromPrivate = await subtle.getPublicKey(
    privateKey,
    ['encapsulateBits']);
  assert(Buffer.from(await subtle.exportKey('raw-public', publicKeyFromPrivate))
    .equals(publicKey));

  const jwk = await subtle.exportKey('jwk', privateKey);
  assert.strictEqual(jwk.kty, 'AKP');
  assert.strictEqual(jwk.alg, algorithm.name);
  assert(Buffer.from(jwk.pub, 'base64url').equals(publicKey));
  assert(Buffer.from(jwk.priv, 'base64url').equals(seed));

  const publicJwk = await subtle.exportKey('jwk', publicKeyFromPrivate);
  assert.deepStrictEqual(publicJwk, {
    kty: 'AKP',
    alg: algorithm.name,
    pub: jwk.pub,
    key_ops: ['encapsulateBits'],
    ext: true,
  });
  const jwkPublicKey = await subtle.importKey(
    'jwk',
    publicJwk,
    algorithm,
    true,
    ['encapsulateBits']);
  assert(Buffer.from(await subtle.exportKey('raw-public', jwkPublicKey))
    .equals(publicKey));

  const jwkPrivateKey = await subtle.importKey(
    'jwk',
    jwk,
    algorithm,
    true,
    ['decapsulateBits']);
  const vectorSharedSecret = await subtle.decapsulateBits(
    algorithm,
    jwkPrivateKey,
    ciphertext);
  assert(Buffer.from(vectorSharedSecret).equals(sharedSecret));

  const implicitRejectionCiphertext = Buffer.from(ciphertext);
  const pqCiphertextLength =
    ciphertext.byteLength - lengths[vector.name].groupElement;
  implicitRejectionCiphertext.fill(0, 0, pqCiphertextLength);
  // Preserve the valid traditional group element so this only exercises
  // ML-KEM implicit rejection.
  assert(implicitRejectionCiphertext.subarray(pqCiphertextLength)
    .equals(ciphertext.subarray(pqCiphertextLength)));

  const implicitRejectionSharedSecret = Buffer.from(
    await subtle.decapsulateBits(
      algorithm,
      privateKey,
      implicitRejectionCiphertext));
  assert.strictEqual(implicitRejectionSharedSecret.byteLength, 32);
  assert(!implicitRejectionSharedSecret.equals(sharedSecret));
  assert(implicitRejectionSharedSecret.equals(Buffer.from(
    await subtle.decapsulateBits(
      algorithm,
      privateKey,
      implicitRejectionCiphertext))));

  const publicKeyOnly = await subtle.importKey(
    'raw-public',
    publicKey,
    algorithm,
    true,
    ['encapsulateBits']);
  const clonedPrivateKey = structuredClone(privateKey);
  const clonedPublicKey = structuredClone(publicKeyOnly);
  const portPrivateKey = await roundTripViaMessageChannel(privateKey);
  const portPublicKey = await roundTripViaMessageChannel(publicKeyOnly);
  assert.deepStrictEqual(clonedPrivateKey, privateKey);
  assert.deepStrictEqual(clonedPublicKey, publicKeyOnly);
  assert.deepStrictEqual(portPrivateKey, privateKey);
  assert.deepStrictEqual(portPublicKey, publicKeyOnly);
  assert(Buffer.from(await subtle.exportKey('raw-seed', clonedPrivateKey))
    .equals(seed));
  assert(Buffer.from(await subtle.exportKey('raw-seed', portPrivateKey))
    .equals(seed));
  assert(Buffer.from((await subtle.exportKey('jwk', clonedPrivateKey)).priv, 'base64url')
    .equals(seed));
  assert(Buffer.from((await subtle.exportKey('jwk', portPrivateKey)).priv, 'base64url')
    .equals(seed));

  const clonedPublicKeyFromPrivate = await subtle.getPublicKey(
    clonedPrivateKey,
    ['encapsulateBits']);
  assert(Buffer.from(await subtle.exportKey('raw-public', clonedPublicKeyFromPrivate))
    .equals(publicKey));

  const clonedVectorSharedSecret = await subtle.decapsulateBits(
    algorithm,
    clonedPrivateKey,
    ciphertext);
  assert(Buffer.from(clonedVectorSharedSecret).equals(sharedSecret));
  const portVectorSharedSecret = await subtle.decapsulateBits(
    algorithm,
    portPrivateKey,
    ciphertext);
  assert(Buffer.from(portVectorSharedSecret).equals(sharedSecret));

  const { publicKey: differentPublicKey } = await subtle.generateKey(
    algorithm,
    true,
    ['encapsulateBits', 'decapsulateBits']);
  assert.notDeepStrictEqual(publicKeyOnly, differentPublicKey);

  const encapsulated = await subtle.encapsulateBits(algorithm, publicKeyOnly);
  const decapsulated = await subtle.decapsulateBits(
    algorithm,
    privateKey,
    encapsulated.ciphertext);
  assert(Buffer.from(decapsulated).equals(Buffer.from(encapsulated.sharedKey)));
  const clonedDecapsulated = await subtle.decapsulateBits(
    algorithm,
    clonedPrivateKey,
    encapsulated.ciphertext);
  assert(Buffer.from(clonedDecapsulated).equals(Buffer.from(encapsulated.sharedKey)));

  const clonedEncapsulated = await subtle.encapsulateBits(algorithm, clonedPublicKey);
  const originalDecapsulated = await subtle.decapsulateBits(
    algorithm,
    privateKey,
    clonedEncapsulated.ciphertext);
  assert(Buffer.from(originalDecapsulated).equals(Buffer.from(clonedEncapsulated.sharedKey)));

  const portEncapsulated = await subtle.encapsulateBits(algorithm, portPublicKey);
  const portDecapsulated = await subtle.decapsulateBits(
    algorithm,
    privateKey,
    portEncapsulated.ciphertext);
  assert(Buffer.from(portDecapsulated).equals(Buffer.from(portEncapsulated.sharedKey)));
}

async function testFailures(vector) {
  const algorithm = { name: vector.name };
  const { seed, publicKey, ciphertext } = buffers(vector);
  const privateKey = await subtle.importKey(
    'raw-seed',
    seed,
    algorithm,
    true,
    ['decapsulateBits']);
  const publicKeyOnly = await subtle.getPublicKey(privateKey, ['encapsulateBits']);
  const jwk = await subtle.exportKey('jwk', privateKey);

  // JWK key usage validation precedes `key_ops` validation.
  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, key_ops: ['encapsulateBits', 'encapsulateBits'] },
      algorithm,
      true,
      ['encapsulateBits']),
    {
      name: 'SyntaxError',
      message: `Unsupported key usage for ${vector.name} key`,
    });
  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, key_ops: ['decapsulateBits', 'decapsulateBits'] },
      algorithm,
      true,
      ['decapsulateBits']),
    { name: 'DataError', message: 'Duplicate key operation' });

  await assert.rejects(
    subtle.importKey('raw-public', Buffer.alloc(publicKey.byteLength - 1), algorithm, true, ['encapsulateBits']),
    { name: 'DataError' });
  await assert.rejects(
    subtle.importKey('raw-seed', Buffer.alloc(seed.byteLength - 1), algorithm, true, ['decapsulateBits']),
    { name: 'DataError' });
  await assert.rejects(
    subtle.importKey('raw-public', publicKey, algorithm, true, ['decapsulateBits']),
    { name: 'SyntaxError' });
  await assert.rejects(
    subtle.importKey('raw-seed', seed, algorithm, true, ['encapsulateBits']),
    { name: 'SyntaxError' });
  await assert.rejects(
    subtle.importKey('raw', publicKey, algorithm, true, ['encapsulateBits']),
    { name: 'NotSupportedError' });
  await assert.rejects(
    subtle.importKey('raw-private', seed, algorithm, true, ['decapsulateBits']),
    { name: 'NotSupportedError' });
  await assert.rejects(
    subtle.exportKey('spki', publicKeyOnly),
    { name: 'NotSupportedError' });
  await assert.rejects(
    subtle.exportKey('pkcs8', privateKey),
    { name: 'NotSupportedError' });
  await assert.rejects(
    subtle.decapsulateBits(algorithm, privateKey, Buffer.alloc(ciphertext.byteLength - 1)),
    { name: 'OperationError' });

  const badCiphertext = Buffer.from(ciphertext);
  badCiphertext.fill(
    0,
    badCiphertext.byteLength - lengths[vector.name].groupElement);
  await assert.rejects(
    subtle.decapsulateBits(algorithm, privateKey, badCiphertext),
    { name: 'OperationError' });

  if (vector.name !== 'MLKEM768-X25519') {
    const groupElementLength = lengths[vector.name].groupElement;
    const nonCanonicalPublicKey = Buffer.from(publicKey);
    const publicKeyPointOffset =
      nonCanonicalPublicKey.byteLength - groupElementLength;
    assert.strictEqual(nonCanonicalPublicKey[publicKeyPointOffset], 0x04);
    nonCanonicalPublicKey[publicKeyPointOffset] =
      0x06 | (nonCanonicalPublicKey[nonCanonicalPublicKey.byteLength - 1] & 1);
    await assert.rejects(
      subtle.importKey(
        'raw-public',
        nonCanonicalPublicKey,
        algorithm,
        true,
        ['encapsulateBits']),
      { name: 'DataError' });

    const nonCanonicalCiphertext = Buffer.from(ciphertext);
    const ciphertextPointOffset =
      nonCanonicalCiphertext.byteLength - groupElementLength;
    assert.strictEqual(nonCanonicalCiphertext[ciphertextPointOffset], 0x04);
    nonCanonicalCiphertext[ciphertextPointOffset] =
      0x06 | (nonCanonicalCiphertext[nonCanonicalCiphertext.byteLength - 1] & 1);
    await assert.rejects(
      subtle.decapsulateBits(algorithm, privateKey, nonCanonicalCiphertext),
      { name: 'OperationError' });
  }
  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, alg: 'ML-KEM-768' },
      algorithm,
      true,
      ['decapsulateBits']),
    { name: 'DataError' });

  const badPublicKey = Buffer.from(publicKey);
  badPublicKey[0] ^= 1;
  await assert.rejects(
    subtle.importKey(
      'jwk',
      { ...jwk, pub: badPublicKey.toString('base64url') },
      algorithm,
      true,
      ['decapsulateBits']),
    { name: 'DataError' });
}

function testSupports(vector) {
  const algorithm = { name: vector.name };
  assert(SubtleCrypto.supports('generateKey', algorithm));
  assert(SubtleCrypto.supports('importKey', algorithm));
  assert(SubtleCrypto.supports('exportKey', algorithm));
  assert(SubtleCrypto.supports('getPublicKey', algorithm));
  assert(SubtleCrypto.supports('encapsulateBits', algorithm));
  assert(SubtleCrypto.supports('decapsulateBits', algorithm));
  assert(SubtleCrypto.supports('encapsulateKey', algorithm, 'HKDF'));
  assert(SubtleCrypto.supports('decapsulateKey', algorithm, 'HKDF'));
  assert(!SubtleCrypto.supports('sign', algorithm));
}

(async () => {
  await testAlgorithmConfigPrototypePollution();
  for (const vector of vectors) {
    testSupports(vector);
    await testGeneratedRoundTrip(vector);
    await testVectorRoundTrip(vector);
    await testFailures(vector);
  }
})().then(common.mustCall());
