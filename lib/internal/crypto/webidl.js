'use strict';

const {
  ArrayPrototypeIncludes,
  ArrayPrototypePush,
  ArrayPrototypeToSorted,
  MathPow,
  NumberParseInt,
  ObjectPrototypeHasOwnProperty,
  ObjectSetPrototypeOf,
  StringPrototypeCharCodeAt,
  StringPrototypeSplit,
  StringPrototypeToLowerCase,
  TypedArrayPrototypeIncludes,
} = primordials;

const {
  lazyDOMException,
} = require('internal/util');
const {
  getCryptoKeyAlgorithm,
  getCryptoKeyType,
  isCryptoKey,
} = require('internal/crypto/keys');
const {
  bigIntArrayToUnsignedInt,
  validateMaxBufferLength,
  getBufferSourceByteLength,
  getBufferSourceBytes,
  getHashes,
  isFips,
  kNamedCurveAliases,
  validateKmacKeyLength,
} = require('internal/crypto/util');
const {
  converters: webidl,
  createDictionaryConverter,
  createEnumConverter,
  createInterfaceConverter,
  createSequenceConverter,
  requiredArguments,
  type,
} = require('internal/webidl');

function validateByteLength(buf, name, target) {
  if (getBufferSourceByteLength(buf) !== target) {
    throw lazyDOMException(
      `${name} must contain exactly ${target} bytes`,
      'OperationError');
  }
}

function AESLengthValidator(V, dict) {
  if (V !== 128 && V !== 192 && V !== 256)
    throw lazyDOMException(
      'AES key length must be 128, 192, or 256 bits',
      'OperationError');
}

function namedCurveValidator(V, dict) {
  if (!ObjectPrototypeHasOwnProperty(kNamedCurveAliases, V))
    throw lazyDOMException(
      'Unrecognized namedCurve',
      'NotSupportedError');
}

const converters = { __proto__: null, ...webidl };

converters.AlgorithmIdentifier = (V, opts) => {
  // Union for (object or DOMString)
  if (type(V) === 'Object') {
    return converters.object(V, opts);
  }
  return converters.DOMString(V, opts);
};

converters.KeyFormat = createEnumConverter('KeyFormat', [
  'raw',
  'raw-public',
  'raw-seed',
  'raw-secret',
  'raw-private',
  'pkcs8',
  'spki',
  'jwk',
]);

converters.KeyUsage = createEnumConverter('KeyUsage', [
  'encrypt',
  'decrypt',
  'sign',
  'verify',
  'deriveKey',
  'deriveBits',
  'wrapKey',
  'unwrapKey',
  'encapsulateBits',
  'decapsulateBits',
  'encapsulateKey',
  'decapsulateKey',
]);

converters['sequence<KeyUsage>'] = createSequenceConverter(converters.KeyUsage);

converters.HashAlgorithmIdentifier = converters.AlgorithmIdentifier;

/**
 * Builds conversion options for Web Crypto integer members that use Web IDL
 * [EnforceRange]. Keep this helper instead of spreading opts in each member
 * converter so the hot dictionary paths allocate stable-shape objects.
 * @param {object} opts Parent conversion options.
 * @returns {object}
 */
function enforceRangeOptions(opts) {
  return {
    prefix: opts.prefix,
    context: opts.context,
    code: opts.code,
    enforceRange: true,
    clamp: undefined,
    allowShared: undefined,
    allowResizable: undefined,
  };
}

const dictAlgorithm = [
  {
    key: 'name',
    converter: converters.DOMString,
    required: true,
  },
];

converters.Algorithm = createDictionaryConverter(
  'Algorithm', dictAlgorithm);

const validators = { __proto__: null };
const algorithmConverters = { __proto__: null };

// Algorithm.name was converted when selecting the registered dictionary.
// Convert the remaining members on the original object, and keep operation
// validation separate from Web IDL conversion.
function createAlgorithmDictionaryConverter(name, dictionaries) {
  const members = [];
  const fullMembers = [dictAlgorithm];
  const checks = [];
  for (let i = 1; i < dictionaries.length; i++) {
    const dictionary = [];
    const sorted = ArrayPrototypeToSorted(dictionaries[i], (a, b) => (a.key < b.key ? -1 : a.key > b.key ? 1 : 0));
    for (let j = 0; j < sorted.length; j++) {
      const member = sorted[j];
      if (ObjectPrototypeHasOwnProperty(member, 'validator')) {
        ArrayPrototypePush(checks, { key: member.key, validator: member.validator });
      }
      ArrayPrototypePush(dictionary, { ...member, validator: undefined });
    }
    ArrayPrototypePush(members, dictionary);
    ArrayPrototypePush(fullMembers, dictionary);
  }
  validators[name] = (algorithm) => {
    for (let i = 0; i < checks.length; i++) {
      const check = checks[i];
      const value = algorithm[check.key];
      if (value !== undefined)
        check.validator(value, algorithm);
    }
  };
  algorithmConverters[name] = createDictionaryConverter(name, members);
  return createDictionaryConverter(name, fullMembers);
}

converters.BigInteger = webidl.Uint8Array;

const dictRsaKeyGenParams = [
  {
    key: 'modulusLength',
    converter: (V, opts) =>
      converters['unsigned long'](V, enforceRangeOptions(opts)),
    validator: (modulusLength) => {
      const kRsaKeyGenMinimumModulusLength = isFips() ? 2048 : 512;
      if (modulusLength < kRsaKeyGenMinimumModulusLength) {
        throw lazyDOMException(
          `algorithm.modulusLength must be at least ${kRsaKeyGenMinimumModulusLength}`,
          'OperationError');
      }
    },
    required: true,
  },
  {
    key: 'publicExponent',
    converter: converters.BigInteger,
    validator: (publicExponent) => {
      const converted = bigIntArrayToUnsignedInt(publicExponent);

      if (converted < 3) {
        throw lazyDOMException(
          'algorithm.publicExponent must be at least 3',
          'OperationError');
      }

      if (converted % 2 === 0) {
        throw lazyDOMException(
          'algorithm.publicExponent must be odd',
          'OperationError');
      }
    },
    required: true,
  },
];

converters.RsaKeyGenParams = createAlgorithmDictionaryConverter(
  'RsaKeyGenParams', [
    dictAlgorithm,
    dictRsaKeyGenParams,
  ]);

converters.RsaHashedKeyGenParams = createAlgorithmDictionaryConverter(
  'RsaHashedKeyGenParams', [
    dictAlgorithm,
    dictRsaKeyGenParams,
    [
      {
        key: 'hash',
        converter: converters.HashAlgorithmIdentifier,
        required: true,
      },
    ],
  ]);

converters.RsaHashedImportParams = createAlgorithmDictionaryConverter(
  'RsaHashedImportParams', [
    dictAlgorithm,
    [
      {
        key: 'hash',
        converter: converters.HashAlgorithmIdentifier,
        required: true,
      },
    ],
  ]);

converters.NamedCurve = converters.DOMString;

converters.EcKeyImportParams = createAlgorithmDictionaryConverter(
  'EcKeyImportParams', [
    dictAlgorithm,
    [
      {
        key: 'namedCurve',
        converter: converters.NamedCurve,
        validator: namedCurveValidator,
        required: true,
      },
    ],
  ]);

converters.EcKeyGenParams = createAlgorithmDictionaryConverter(
  'EcKeyGenParams', [
    dictAlgorithm,
    [
      {
        key: 'namedCurve',
        converter: converters.NamedCurve,
        validator: namedCurveValidator,
        required: true,
      },
    ],
  ]);

converters.AesKeyGenParams = createAlgorithmDictionaryConverter(
  'AesKeyGenParams', [
    dictAlgorithm,
    [
      {
        key: 'length',
        converter: (V, opts) =>
          converters['unsigned short'](V, enforceRangeOptions(opts)),
        validator: AESLengthValidator,
        required: true,
      },
    ],
  ]);

function validateZeroLength(parameterName) {
  return (V, dict) => {
    if (getBufferSourceByteLength(V)) {
      throw lazyDOMException(
        `Non zero-length ${parameterName} is not supported.`, 'NotSupportedError');
    }
  };
}

function validateCShakeOutputLength(V) {
  if (V % 8 !== 0) {
    throw lazyDOMException(
      'Invalid CShakeParams outputLength',
      'NotSupportedError');
  }
}

const kCShakeFunctionNames = ['KMAC', 'TupleHash', 'ParallelHash'];

function validateCShakeFunctionName(V, dict) {
  const length = getBufferSourceByteLength(V);
  if (length === 0) return;

  if (ArrayPrototypeIncludes(getHashes(), StringPrototypeToLowerCase(dict.name))) {
    const bytes = getBufferSourceBytes(V);
    for (let i = 0; i < kCShakeFunctionNames.length; i++) {
      const functionName = kCShakeFunctionNames[i];
      if (length !== functionName.length) continue;

      let j = 0;
      for (; j < length; j++) {
        if (bytes[j] !== StringPrototypeCharCodeAt(functionName, j)) break;
      }
      if (j === length) return;
    }
  }

  throw lazyDOMException(
    'Unsupported CShakeParams functionName',
    'NotSupportedError');
}

function validateCShakeCustomization(V, dict) {
  if (getBufferSourceByteLength(V) === 0) return;
  if (!ArrayPrototypeIncludes(getHashes(), StringPrototypeToLowerCase(dict.name)))
    throw lazyDOMException(
      'Unsupported CShakeParams customization',
      'NotSupportedError');
  validateMaxBufferLength(V, 'CShakeParams.customization', 512);
  if (TypedArrayPrototypeIncludes(getBufferSourceBytes(V), 0))
    throw lazyDOMException(
      'Unsupported CShakeParams customization',
      'NotSupportedError');
}

converters.RsaPssParams = createAlgorithmDictionaryConverter(
  'RsaPssParams', [
    dictAlgorithm,
    [
      {
        key: 'saltLength',
        converter: (V, opts) =>
          converters['unsigned long'](V, enforceRangeOptions(opts)),
        required: true,
      },
    ],
  ]);

converters.RsaOaepParams = createAlgorithmDictionaryConverter(
  'RsaOaepParams', [
    dictAlgorithm,
    [
      {
        key: 'label',
        converter: converters.BufferSource,
      },
    ],
  ]);

converters.EcdsaParams = createAlgorithmDictionaryConverter(
  'EcdsaParams', [
    dictAlgorithm,
    [
      {
        key: 'hash',
        converter: converters.HashAlgorithmIdentifier,
        required: true,
      },
    ],
  ]);

function validateHmacKeyLength(parameterName, zeroError) {
  return (V) => {
    if (V === 0)
      throw lazyDOMException(`${parameterName} cannot be 0`, zeroError);
  };
}

const kHmacDictionaries = [
  ['HmacKeyGenParams', 'OperationError'],
  ['HmacImportParams', 'DataError'],
];
for (let i = 0; i < kHmacDictionaries.length; i++) {
  const { 0: name, 1: zeroError } = kHmacDictionaries[i];
  converters[name] = createAlgorithmDictionaryConverter(
    name, [
      dictAlgorithm,
      [
        {
          key: 'hash',
          converter: converters.HashAlgorithmIdentifier,
          required: true,
        },
        {
          key: 'length',
          converter: (V, opts) =>
            converters['unsigned long'](V, enforceRangeOptions(opts)),
          validator: validateHmacKeyLength(`${name}.length`, zeroError),
        },
      ],
    ]);
}

const simpleDomStringKey = (key) => ({ key, converter: converters.DOMString });

converters.RsaOtherPrimesInfo = createDictionaryConverter(
  'RsaOtherPrimesInfo', [
    simpleDomStringKey('r'),
    simpleDomStringKey('d'),
    simpleDomStringKey('t'),
  ]);
converters['sequence<RsaOtherPrimesInfo>'] = createSequenceConverter(
  converters.RsaOtherPrimesInfo);

converters.JsonWebKey = createDictionaryConverter(
  'JsonWebKey', [
    simpleDomStringKey('kty'),
    simpleDomStringKey('use'),
    {
      key: 'key_ops',
      converter: converters['sequence<DOMString>'],
    },
    simpleDomStringKey('alg'),
    {
      key: 'ext',
      converter: converters.boolean,
    },
    simpleDomStringKey('crv'),
    simpleDomStringKey('x'),
    simpleDomStringKey('y'),
    simpleDomStringKey('d'),
    simpleDomStringKey('n'),
    simpleDomStringKey('e'),
    simpleDomStringKey('p'),
    simpleDomStringKey('q'),
    simpleDomStringKey('dp'),
    simpleDomStringKey('dq'),
    simpleDomStringKey('qi'),
    {
      key: 'oth',
      converter: converters['sequence<RsaOtherPrimesInfo>'],
    },
    simpleDomStringKey('k'),
    simpleDomStringKey('pub'),
    simpleDomStringKey('priv'),
  ]);

converters.HkdfParams = createAlgorithmDictionaryConverter(
  'HkdfParams', [
    dictAlgorithm,
    [
      {
        key: 'hash',
        converter: converters.HashAlgorithmIdentifier,
        required: true,
      },
      {
        key: 'salt',
        converter: converters.BufferSource,
        required: true,
      },
      {
        key: 'info',
        converter: converters.BufferSource,
        validator: (V, dict) => validateMaxBufferLength(V, 'algorithm.info', 1024),
        required: true,
      },
    ],
  ]);

converters.CShakeParams = createAlgorithmDictionaryConverter(
  'CShakeParams', [
    dictAlgorithm,
    [
      {
        key: 'outputLength',
        converter: (V, opts) =>
          converters['unsigned long'](V, enforceRangeOptions(opts)),
        validator: validateCShakeOutputLength,
        required: true,
      },
      {
        key: 'functionName',
        converter: converters.BufferSource,
        validator: validateCShakeFunctionName,
      },
      {
        key: 'customization',
        converter: converters.BufferSource,
        validator: validateCShakeCustomization,
      },
    ],
  ]);

converters.Pbkdf2Params = createAlgorithmDictionaryConverter(
  'Pbkdf2Params', [
    dictAlgorithm,
    [
      {
        key: 'salt',
        converter: converters.BufferSource,
        required: true,
      },
      {
        key: 'iterations',
        converter: (V, opts) =>
          converters['unsigned long'](V, enforceRangeOptions(opts)),
        required: true,
      },
      {
        key: 'hash',
        converter: converters.HashAlgorithmIdentifier,
        required: true,
      },
    ],
  ]);

converters.AesDerivedKeyParams = createAlgorithmDictionaryConverter(
  'AesDerivedKeyParams', [
    dictAlgorithm,
    [
      {
        key: 'length',
        converter: (V, opts) =>
          converters['unsigned short'](V, enforceRangeOptions(opts)),
        validator: AESLengthValidator,
        required: true,
      },
    ],
  ]);

converters.AesCbcParams = createAlgorithmDictionaryConverter(
  'AesCbcParams', [
    dictAlgorithm,
    [
      {
        key: 'iv',
        converter: converters.BufferSource,
        validator: (V, dict) => validateByteLength(V, 'algorithm.iv', 16),
        required: true,
      },
    ],
  ]);

converters.AeadParams = createAlgorithmDictionaryConverter(
  'AeadParams', [
    dictAlgorithm,
    [
      {
        key: 'iv',
        converter: converters.BufferSource,
        validator: (V, dict) => {
          switch (StringPrototypeToLowerCase(dict.name)) {
            case 'chacha20-poly1305':
              validateByteLength(V, 'algorithm.iv', 12);
              break;
            case 'aes-gcm':
              validateMaxBufferLength(V, 'algorithm.iv');
              break;
            case 'aes-ocb':
              if (getBufferSourceByteLength(V) > 15) {
                throw lazyDOMException(
                  'AES-OCB algorithm.iv must be no more than 15 bytes',
                  'OperationError');
              }
              break;
          }
        },
        required: true,
      },
      {
        key: 'additionalData',
        converter: converters.BufferSource,
        validator: (V, dict) => validateMaxBufferLength(V, 'algorithm.additionalData'),
      },
      {
        key: 'tagLength',
        converter: (V, opts) =>
          converters.octet(V, enforceRangeOptions(opts)),
        validator: (V, dict) => {
          switch (StringPrototypeToLowerCase(dict.name)) {
            case 'chacha20-poly1305':
              if (V !== 128) {
                throw lazyDOMException(
                  `${V} is not a valid ChaCha20-Poly1305 tag length`,
                  'OperationError');
              }
              break;
            case 'aes-gcm':
              if (!ArrayPrototypeIncludes([32, 64, 96, 104, 112, 120, 128], V)) {
                throw lazyDOMException(
                  `${V} is not a valid AES-GCM tag length`,
                  'OperationError');
              }
              break;
            case 'aes-ocb':
              if (!ArrayPrototypeIncludes([64, 96, 128], V)) {
                throw lazyDOMException(
                  `${V} is not a valid AES-OCB tag length`,
                  'OperationError');
              }
              break;
          }
        },
      },
    ],
  ]);

converters.AesCtrParams = createAlgorithmDictionaryConverter(
  'AesCtrParams', [
    dictAlgorithm,
    [
      {
        key: 'counter',
        converter: converters.BufferSource,
        validator: (V, dict) => validateByteLength(V, 'algorithm.counter', 16),
        required: true,
      },
      {
        key: 'length',
        converter: (V, opts) =>
          converters.octet(V, enforceRangeOptions(opts)),
        validator: (V, dict) => {
          if (V === 0 || V > 128)
            throw lazyDOMException(
              'AES-CTR algorithm.length must be between 1 and 128',
              'OperationError');
        },
        required: true,
      },
    ],
  ]);

converters.CryptoKey = createInterfaceConverter(
  'CryptoKey', isCryptoKey);

converters.EcdhKeyDeriveParams = createAlgorithmDictionaryConverter(
  'EcdhKeyDeriveParams', [
    dictAlgorithm,
    [
      {
        key: 'public',
        converter: converters.CryptoKey,
        validator: (V, dict) => {
          if (getCryptoKeyType(V) !== 'public')
            throw lazyDOMException(
              'algorithm.public must be a public key', 'InvalidAccessError');

          if (StringPrototypeToLowerCase(getCryptoKeyAlgorithm(V).name) !== StringPrototypeToLowerCase(dict.name))
            throw lazyDOMException(
              'key algorithm mismatch',
              'InvalidAccessError');
        },
        required: true,
      },
    ],
  ]);

converters.ContextParams = createAlgorithmDictionaryConverter(
  'ContextParams', [
    dictAlgorithm,
    [
      {
        key: 'context',
        converter: converters.BufferSource,
        validator(V, dict) {
          const validateLength = (V) =>
            validateMaxBufferLength(V, 'ContextParams.context', 255);

          if (process.features.openssl_is_boringssl) {
            this.validator = validateLength;
          } else {
            let { 0: major, 1: minor } =
              StringPrototypeSplit(process.versions.openssl, '.');
            major = NumberParseInt(major, 10);
            minor = NumberParseInt(minor, 10);
            if (major > 3 || (major === 3 && minor >= 2)) {
              this.validator = validateLength;
            } else {
              const validateEmpty = validateZeroLength('ContextParams.context');
              this.validator = (V, dict) => {
                validateLength(V);
                validateEmpty(V, dict);
              };
            }
          }
          this.validator(V, dict);
        },
      },
    ],
  ]);

converters.Argon2Params = createAlgorithmDictionaryConverter(
  'Argon2Params', [
    dictAlgorithm,
    [
      {
        key: 'nonce',
        converter: converters.BufferSource,
        validator: (V) => {
          if (getBufferSourceByteLength(V) < 8) {
            throw lazyDOMException('nonce must be at least 8 bytes', 'OperationError');
          }
        },
        required: true,
      },
      {
        key: 'parallelism',
        converter: (V, opts) =>
          converters['unsigned long'](V, enforceRangeOptions(opts)),
        validator: (V, dict) => {
          if (V === 0 || V > MathPow(2, 24) - 1) {
            throw lazyDOMException(
              'parallelism must be > 0 and <= 16777215',
              'OperationError');
          }
        },
        required: true,
      },
      {
        key: 'memory',
        converter: (V, opts) =>
          converters['unsigned long'](V, enforceRangeOptions(opts)),
        validator: (V, dict) => {
          if (V < 8 * dict.parallelism) {
            throw lazyDOMException(
              'memory must be at least 8 times the degree of parallelism',
              'OperationError');
          }
        },
        required: true,
      },
      {
        key: 'passes',
        converter: (V, opts) =>
          converters['unsigned long'](V, enforceRangeOptions(opts)),
        validator: (V) => {
          if (V === 0) {
            throw lazyDOMException('passes must be > 0', 'OperationError');
          }
        },
        required: true,
      },
      {
        key: 'version',
        converter: (V, opts) =>
          converters.octet(V, enforceRangeOptions(opts)),
        validator: (V, dict) => {
          if (V !== 0x13) {
            throw lazyDOMException(
              `${V} is not a valid Argon2 version`,
              'OperationError');
          }
        },
      },
      {
        key: 'secretValue',
        converter: converters.BufferSource,
      },
      {
        key: 'associatedData',
        converter: converters.BufferSource,
      },
    ],
  ]);

const kKmacDictionaries = ['KmacKeyGenParams', 'KmacImportParams'];
for (let i = 0; i < kKmacDictionaries.length; i++) {
  const name = kKmacDictionaries[i];
  converters[name] = createAlgorithmDictionaryConverter(
    name, [
      dictAlgorithm,
      [
        {
          key: 'length',
          converter: (V, opts) =>
            converters['unsigned long'](V, enforceRangeOptions(opts)),
          validator: validateKmacKeyLength,
        },
      ],
    ]);
}

converters.KmacParams = createAlgorithmDictionaryConverter(
  'KmacParams', [
    dictAlgorithm,
    [
      {
        key: 'outputLength',
        converter: (V, opts) =>
          converters['unsigned long'](V, enforceRangeOptions(opts)),
        validator: (V) => {
          if (V % 8 !== 0 || (V === 0 && isFips()))
            throw lazyDOMException(
              'Invalid KmacParams outputLength',
              'NotSupportedError');
        },
        required: true,
      },
      {
        key: 'customization',
        converter: converters.BufferSource,
      },
    ],
  ]);

converters.KangarooTwelveParams = createAlgorithmDictionaryConverter(
  'KangarooTwelveParams', [
    dictAlgorithm,
    [
      {
        key: 'outputLength',
        converter: (V, opts) =>
          converters['unsigned long'](V, enforceRangeOptions(opts)),
        validator: (V, opts) => {
          if (V === 0 || V % 8)
            throw lazyDOMException('Invalid KangarooTwelveParams outputLength', 'OperationError');
        },
        required: true,
      },
      {
        key: 'customization',
        converter: converters.BufferSource,
        validator: (V, opts) => validateMaxBufferLength(V, 'KangarooTwelveParams.customization', 512),
      },
    ],
  ]);

converters.TurboShakeParams = createAlgorithmDictionaryConverter(
  'TurboShakeParams', [
    dictAlgorithm,
    [
      {
        key: 'outputLength',
        converter: (V, opts) =>
          converters['unsigned long'](V, enforceRangeOptions(opts)),
        validator: (V, opts) => {
          if (V === 0 || V % 8)
            throw lazyDOMException('Invalid TurboShakeParams outputLength', 'OperationError');
        },
        required: true,
      },
      {
        key: 'domainSeparation',
        converter: (V, opts) =>
          converters.octet(V, enforceRangeOptions(opts)),
        validator: (V) => {
          if (V < 0x01 || V > 0x7F) {
            throw lazyDOMException(
              'TurboShakeParams.domainSeparation must be in range 0x01-0x7f',
              'OperationError');
          }
        },
      },
    ],
  ]);

module.exports = {
  // Spread into fast-property objects before detaching their prototypes.
  algorithmConverters: ObjectSetPrototypeOf({ ...algorithmConverters }, null),
  converters,
  requiredArguments,
  validators: ObjectSetPrototypeOf({ ...validators }, null),
};
