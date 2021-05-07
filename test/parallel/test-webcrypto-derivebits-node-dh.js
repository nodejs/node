'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (common.hasOpenSSL3)
  common.skip('temporarily skipping for OpenSSL 3.0-alpha15');

const assert = require('assert');
const { subtle } = require('crypto').webcrypto;

const kTestData = {
  pkcs8: '308203260201003082019706092a864886f70d010301308201880282018100ff' +
         'ffffffffffffffc90fdaa22168c234c4c6628b80dc1cd129024e088a67cc7402' +
         '0bbea63b139b22514a08798e3404ddef9519b3cd3a431b302b0a6df25f14374f' +
         'e1356d6d51c245e485b576625e7ec6f44c42e9a637ed6b0bff5cb6f406b7edee' +
         '386bfb5a899fa5ae9f24117c4b1fe649286651ece45b3dc2007cb8a163bf0598' +
         'da48361c55d39a69163fa8fd24cf5f83655d23dca3ad961c62f356208552bb9e' +
         'd529077096966d670c354e4abc9804f1746c08ca18217c32905e462e36ce3be3' +
         '9e772c180e86039b2783a2ec07a28fb5c55df06f4c52c9de2bcbf69558171839' +
         '95497cea956ae515d2261898fa051015728e5a8aaac42dad33170d04507a33a8' +
         '5521abdf1cba64ecfb850458dbef0a8aea71575d060c7db3970f85a6e1e4c7ab' +
         'f5ae8cdb0933d71e8c94e04a25619dcee3d2261ad2ee6bf12ffa06d98a0864d8' +
         '7602733ec86a64521f2b18177b200cbbe117577a615d6c770988c0bad946e208' +
         'e24fa074e5ab3143db5bfce0fd108e4b82d120a93ad2caffffffffffffffff02' +
         '010204820184028201806ba21bec44fe29bee2d1f6f5d717b9af1c62973b342c' +
         '28b850d2e39b31abf3ce7f58a26da0cdf538b3328648ee8738e49434bdf697ff' +
         '2ac5da64d308fa0abbd75f0554bcef58fd1a688bf93af00ffbdb9ba05558a8e6' +
         '16db9818769e2c7e37ba0c07bca10fffd19cb7aa44876170f0a8fe0c1bdba2f9' +
         'b0606d479796e1b6bdab153188b03f684584fd909814c5055fc184eaf05577f7' +
         'f447fcc2dd90889ab853830993d8fbf087f942d8e7a9f331570e7eee5954fb7a' +
         'a6920f4df2f8a33d5cb59961162b1216a4382f984cce02512f0100e20f15e480' +
         'a9d19bb01414a75c74d0854595e58ed060f7bb0f4b451f82b476dc039cc1bb8c' +
         '1a05999e6abb20915a3cfca40f314538c00f42d0c4f2070cb163788d6dea0adb' +
         '03c75d8001c7057ef61e60b407272adffc2669b82e634ebebda45826229bb2f8' +
         'ed742ee97429e34de06c41c25563025285d5f8a43acdc57cf12779222ae125d5' +
         '438857d6bf6341815b9aebf6878fd23944cfd240e74caea13419163fde5ec1ec' +
         'fe2fb2e740b9301a9c04',
  spki: '308203253082019706092a864886f70d010301308201880282018100fffffffff' +
        'fffffffc90fdaa22168c234c4c6628b80dc1cd129024e088a67cc74020bbea63b' +
        '139b22514a08798e3404ddef9519b3cd3a431b302b0a6df25f14374fe1356d6d5' +
        '1c245e485b576625e7ec6f44c42e9a637ed6b0bff5cb6f406b7edee386bfb5a89' +
        '9fa5ae9f24117c4b1fe649286651ece45b3dc2007cb8a163bf0598da48361c55d' +
        '39a69163fa8fd24cf5f83655d23dca3ad961c62f356208552bb9ed52907709696' +
        '6d670c354e4abc9804f1746c08ca18217c32905e462e36ce3be39e772c180e860' +
        '39b2783a2ec07a28fb5c55df06f4c52c9de2bcbf6955817183995497cea956ae5' +
        '15d2261898fa051015728e5a8aaac42dad33170d04507a33a85521abdf1cba64e' +
        'cfb850458dbef0a8aea71575d060c7db3970f85a6e1e4c7abf5ae8cdb0933d71e' +
        '8c94e04a25619dcee3d2261ad2ee6bf12ffa06d98a0864d87602733ec86a64521' +
        'f2b18177b200cbbe117577a615d6c770988c0bad946e208e24fa074e5ab3143db' +
        '5bfce0fd108e4b82d120a93ad2caffffffffffffffff020102038201860002820' +
        '1810098b01807f77a722cd30fe0390f8cdd0b3f7e0fb33fd3e0ca3d573a7b481e' +
        'dc0fc9f7bc58bfc4509f34efe118ba48bcefbbd57c4878e87b4d0fbb4b661d771' +
        '19055cf190b5e8ad05c4821ff0445f4e12049e708ee83e2272c2c269b8b3bfa1b' +
        '37df8dc4ec77a430ed6b89beb69093ceeca48497d8ab3ceac25429df58e96d2df' +
        'd01a94b1d164697b93475ed3d5962fd7c300de959474568aecc15c880c36b14a7' +
        '9c2144ad4207c9760c11fbcc9ccbb29727a7f5a7789d47f1fa7a154289f83c528' +
        '1fb97e01c37f937a25b4057017961ebd52b936f6b8f6d9ad35e3e094c3850cde2' +
        '9e98895eaa9b56bff17dc6d8c03edce51c0aa009864da9cbe49a2063bddc64f11' +
        '67beb0ab1c5d9bccf9a10078d0236237986f53c4074f30399deb829c544026fe0' +
        '532e54e1bc9d933bd4db8dbfd43cf7839a9f64c135d2188896294d54cb7812e6e' +
        '9586a03e91893edf7240011e8cda8f59a4dde0ce6d3c40d85847a3985fa25967f' +
        'a2c9598b269ac28da067704f15eb5bb4d8afdea8c67f56f9b604e172bc',
  result: '42e77c77568f4387858771b006bebf213a2894efed8efdcfe23640bbd6df765' +
          '1b2afa9f5acfba5b1d3db5401fd3427cd9226f6c2fdbd2b8fd7948e12cc7e016' +
          'eb2ed0e0fb02ce434c68c5d511ded05c1150718cf4c8b9db0adcc639a8b52b74' +
          'c2c90e2df5f6f55462d38e2d2769b4bd23bf0db8b4a82253addc1d5d6d19289d' +
          '9e60ddabe1203aa3f45956f4dd32683f55fc2f7400e92712e5bb4dea737e03ad' +
          '4b94640ccd63a0b8485dfa63f3b98232a34354aa91aebb3e86bb67a48b50ac21' +
          'e62dbce80c0913a9dba92c1c2333f4a43efe9c6c24ce23c252c7ff37e9e74a78' +
          '6a8c5b82453fda8b6b40463461d22765e26d4f444272254ee6a492d3e6737e52' +
          'a94440aeddde3c0b449c52ca64bdc698f683ba6cb27794b84edcab7694cadfe8' +
          'f76ce94bf127ca42323b1995a863bcecab455f5d6283eb0eed44942aad3d1a46' +
          '79713ed4757917f95d1a61ecae45b9978a863b16e199418bfe6e98435ad146a4' +
          '7bd9c8cad9f787a29888954ae58fa683ea163921eb9d3e7a5aa2f52f74ec15333',
};

async function prepareKeys() {
  const [
    privateKey,
    publicKey,
  ] = await Promise.all([
    subtle.importKey(
      'pkcs8',
      Buffer.from(kTestData.pkcs8, 'hex'),
      { name: 'NODE-DH' },
      true,
      ['deriveKey', 'deriveBits']),
    subtle.importKey(
      'spki',
      Buffer.from(kTestData.spki, 'hex'),
      { name: 'NODE-DH' },
      true, []),
  ]);
  return {
    privateKey,
    publicKey,
    result: kTestData.result,
  };
}

(async function() {
  const {
    publicKey,
    privateKey,
    result
  } = await prepareKeys();

  {
    // Good parameters
    const bits = await subtle.deriveBits({
      name: 'NODE-DH',
      public: publicKey
    }, privateKey, null);

    assert(bits instanceof ArrayBuffer);
    assert.strictEqual(Buffer.from(bits).toString('hex'), result);
  }

  {
    // Case insensitivity
    const bits = await subtle.deriveBits({
      name: 'node-dH',
      public: publicKey
    }, privateKey, null);

    assert.strictEqual(Buffer.from(bits).toString('hex'), result);
  }

  {
    // Short Result
    const bits = await subtle.deriveBits({
      name: 'NODE-DH',
      public: publicKey
    }, privateKey, 16);

    assert.strictEqual(
      Buffer.from(bits).toString('hex'),
      result.slice(0, 4));
  }

  {
    // Too long result
    await assert.rejects(subtle.deriveBits({
      name: 'NODE-DH',
      public: publicKey
    }, privateKey, result.length * 16), {
      message: /derived bit length is too small/
    });
  }

  {
    // Non-multiple of 8
    const bits = await subtle.deriveBits({
      name: 'NODE-DH',
      public: publicKey
    }, privateKey, 15);

    assert.strictEqual(
      Buffer.from(bits).toString('hex'),
      result.slice(0, 2));
  }

  // Error tests
  {
    // Missing public property
    await assert.rejects(
      subtle.deriveBits(
        { name: 'NODE-DH' },
        privateKey,
        null),
      { code: 'ERR_INVALID_ARG_TYPE' });
  }

  {
    // The public property is not a CryptoKey
    await assert.rejects(
      subtle.deriveBits(
        {
          name: 'NODE-DH',
          public: { message: 'Not a CryptoKey' }
        },
        privateKey,
        null),
      { code: 'ERR_INVALID_ARG_TYPE' });
  }

  {
    // Incorrect public key algorithm
    const { publicKey } = await subtle.generateKey(
      {
        name: 'ECDSA',
        namedCurve: 'P-521'
      }, false, ['verify']);

    await assert.rejects(subtle.deriveBits({
      name: 'NODE-DH',
      public: publicKey
    }, privateKey, null), {
      message: /Keys must be DH keys/
    });
  }

  {
    // Private key does not have correct usages
    const privateKey = await subtle.importKey(
      'pkcs8',
      Buffer.from(kTestData.pkcs8, 'hex'),
      {
        name: 'NODE-DH',
      }, false, ['deriveKey']);

    await assert.rejects(subtle.deriveBits({
      name: 'NODE-DH',
      public: publicKey,
    }, privateKey, null), {
      message: /baseKey does not have deriveBits usage/
    });
  }
})().then(common.mustCall());
