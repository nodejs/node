'use strict';

function testSupportsMethod() {
  test(() => {
    assert_true(
      typeof SubtleCrypto.supports === 'function',
      'SubtleCrypto.supports should be a function'
    );
  }, 'SubtleCrypto.supports method exists');
}

function runSupportsTests(algorithms, operations) {
  for (const [algorithmName, algorithmInfo] of Object.entries(algorithms)) {
    for (const operation of operations) {
      promise_test(async (t) => {
        const isSupported = algorithmInfo.operations.includes(operation);

        let algorithm;
        let lengthOrAdditionalAlgorithm;
        switch (operation) {
          case 'generateKey':
            algorithm = algorithmInfo.keyGenParams || algorithmName;
            break;
          case 'importKey':
            algorithm = algorithmInfo.importParams || algorithmName;
            break;
          case 'sign':
          case 'verify':
            algorithm = algorithmInfo.signParams || algorithmName;
            break;
          case 'encrypt':
          case 'decrypt':
            algorithm = algorithmInfo.encryptParams || algorithmName;
            break;
          case 'deriveBits':
            algorithm = algorithmInfo.deriveBitsParamsFactory ?
              await algorithmInfo.deriveBitsParamsFactory() :
              algorithmInfo.deriveBitsParams || algorithmName;
            if (algorithmName === 'PBKDF2' || algorithmName === 'HKDF') {
              lengthOrAdditionalAlgorithm = 256;
            }
            break;
          case 'digest':
            algorithm = algorithmName;
            break;
          case 'encapsulateKey':
          case 'encapsulateBits':
          case 'decapsulateKey':
          case 'decapsulateBits':
            algorithm = algorithmName;
            if (operation === 'encapsulateKey' || operation === 'decapsulateKey') {
              lengthOrAdditionalAlgorithm = { name: 'AES-GCM', length: 256 };
            }
            break;
          default:
            algorithm = algorithmName;
        }

        const result = SubtleCrypto.supports(
          operation,
          algorithm,
          lengthOrAdditionalAlgorithm
        );

        if (isSupported) {
          assert_true(result, `${algorithmName} should support ${operation}`);
        } else {
          assert_false(result, `${algorithmName} should not support ${operation}`);
        }
      }, `supports(${operation}, ${algorithmName})`);
    }
  }
}
