'use strict';

const {
  ArrayPrototypeIncludes,
  MathPow,
  NumberParseInt,
  ObjectPrototypeHasOwnProperty,
  StringPrototypeStartsWith,
  StringPrototypeToLowerCase,
} = primordials;

const {
  lazyDOMException,
  kEmptyObject,
} = require('internal/util');
const { CryptoKey } = require('internal/crypto/webcrypto');
const {
  getCryptoKeyAlgorithm,
  getCryptoKeyType,
} = require('internal/crypto/keys');
const {
  validateMaxBufferLength,
  kNamedCurveAliases,
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
  if (buf.byteLength !== target) {
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

/**
 * @param {string | object} V - The hash algorithm identifier (string or object).
 * @param {string} label - The dictionary name for the error message.
 */
function ensureSHA(V, label) {
  const name = typeof V === 'string' ? V : V.name;
  if (typeof name !== 'string' ||
      !StringPrototypeStartsWith(StringPrototypeToLowerCase(name), 'sha'))
    throw lazyDOMException(
      `Only SHA hashes are supported in ${label}`, 'NotSupportedError');
}

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

// TODO(panva): Reject resizable backing stores in a semver-major with:
// converters.BigInteger = webidl.Uint8Array;
converters.BigInteger = (V, opts = kEmptyObject) => {
  return webidl.Uint8Array(V, {
    prefix: opts.prefix,
    context: opts.context,
    code: opts.code,
    allowResizable: true,
    allowShared: false,
  });
};

// TODO(panva): Reject resizable backing stores in a semver-major by
// removing this altogether.
converters.BufferSource = (V, opts = kEmptyObject) => {
  return webidl.BufferSource(V, {
    prefix: opts.prefix,
    context: opts.context,
    code: opts.code,
    allowResizable: opts.allowResizable === undefined ?
      true : opts.allowResizable,
    allowShared: opts.allowShared,
  });
};

const dictRsaKeyGenParams = [
  {
    key: 'modulusLength',
    converter: (V, opts) =>
      converters['unsigned long'](V, enforceRangeOptions(opts)),
    required: true,
  },
  {
    key: 'publicExponent',
    converter: converters.BigInteger,
    required: true,
  },
];

converters.RsaKeyGenParams = createDictionaryConverter(
  'RsaKeyGenParams', [
    dictAlgorithm,
    dictRsaKeyGenParams,
  ]);

converters.RsaHashedKeyGenParams = createDictionaryConverter(
  'RsaHashedKeyGenParams', [
    dictAlgorithm,
    dictRsaKeyGenParams,
    [
      {
        key: 'hash',
        converter: converters.HashAlgorithmIdentifier,
        validator: (V, dict) => ensureSHA(V, 'RsaHashedKeyGenParams'),
        required: true,
      },
    ],
  ]);

converters.RsaHashedImportParams = createDictionaryConverter(
  'RsaHashedImportParams', [
    dictAlgorithm,
    [
      {
        key: 'hash',
        converter: converters.HashAlgorithmIdentifier,
        validator: (V, dict) => ensureSHA(V, 'RsaHashedImportParams'),
        required: true,
      },
    ],
  ]);

converters.NamedCurve = converters.DOMString;

converters.EcKeyImportParams = createDictionaryConverter(
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

converters.EcKeyGenParams = createDictionaryConverter(
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

converters.AesKeyGenParams = createDictionaryConverter(
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
    if (V.byteLength) {
      throw lazyDOMException(
        `Non zero-length ${parameterName} is not supported.`, 'NotSupportedError');
    }
  };
}

converters.RsaPssParams = createDictionaryConverter(
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

converters.RsaOaepParams = createDictionaryConverter(
  'RsaOaepParams', [
    dictAlgorithm,
    [
      {
        key: 'label',
        converter: converters.BufferSource,
      },
    ],
  ]);

converters.EcdsaParams = createDictionaryConverter(
  'EcdsaParams', [
    dictAlgorithm,
    [
      {
        key: 'hash',
        converter: converters.HashAlgorithmIdentifier,
        validator: (V, dict) => ensureSHA(V, 'EcdsaParams'),
        required: true,
      },
    ],
  ]);

for (const { 0: name, 1: zeroError } of [['HmacKeyGenParams', 'OperationError'], ['HmacImportParams', 'DataError']]) {
  converters[name] = createDictionaryConverter(
    name, [
      dictAlgorithm,
      [
        {
          key: 'hash',
          converter: converters.HashAlgorithmIdentifier,
          validator: (V, dict) => ensureSHA(V, name),
          required: true,
        },
        {
          key: 'length',
          converter: (V, opts) =>
            converters['unsigned long'](V, enforceRangeOptions(opts)),
          validator: validateMacKeyLength(`${name}.length`, zeroError),
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

converters.HkdfParams = createDictionaryConverter(
  'HkdfParams', [
    dictAlgorithm,
    [
      {
        key: 'hash',
        converter: converters.HashAlgorithmIdentifier,
        validator: (V, dict) => ensureSHA(V, 'HkdfParams'),
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

converters.CShakeParams = createDictionaryConverter(
  'CShakeParams', [
    dictAlgorithm,
    [
      {
        key: 'outputLength',
        converter: (V, opts) =>
          converters['unsigned long'](V, enforceRangeOptions(opts)),
        validator: (V, opts) => {
          // The Web Crypto spec allows for SHAKE output length that are not multiples of
          // 8. We don't.
          if (V % 8)
            throw lazyDOMException('Unsupported CShakeParams outputLength', 'NotSupportedError');
        },
        required: true,
      },
      {
        key: 'functionName',
        converter: converters.BufferSource,
        validator: validateZeroLength('CShakeParams.functionName'),
      },
      {
        key: 'customization',
        converter: converters.BufferSource,
        validator: validateZeroLength('CShakeParams.customization'),
      },
    ],
  ]);

converters.Pbkdf2Params = createDictionaryConverter(
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
        validator: (V, dict) => {
          if (V === 0)
            throw lazyDOMException('iterations cannot be zero', 'OperationError');
        },
        required: true,
      },
      {
        key: 'hash',
        converter: converters.HashAlgorithmIdentifier,
        validator: (V, dict) => ensureSHA(V, 'Pbkdf2Params'),
        required: true,
      },
    ],
  ]);

converters.AesDerivedKeyParams = createDictionaryConverter(
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

converters.AesCbcParams = createDictionaryConverter(
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

converters.AeadParams = createDictionaryConverter(
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
              if (V.byteLength > 15) {
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

converters.AesCtrParams = createDictionaryConverter(
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
  'CryptoKey', CryptoKey.prototype);

converters.EcdhKeyDeriveParams = createDictionaryConverter(
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

converters.ContextParams = createDictionaryConverter(
  'ContextParams', [
    dictAlgorithm,
    [
      {
        key: 'context',
        converter: converters.BufferSource,
        validator(V, dict) {
          let { 0: major, 1: minor } = process.versions.openssl.split('.');
          major = NumberParseInt(major, 10);
          minor = NumberParseInt(minor, 10);
          if (major > 3 || (major === 3 && minor >= 2)) {
            this.validator = undefined;
          } else {
            this.validator = validateZeroLength('ContextParams.context');
            this.validator(V, dict);
          }
        },
      },
    ],
  ]);

converters.Argon2Params = createDictionaryConverter(
  'Argon2Params', [
    dictAlgorithm,
    [
      {
        key: 'nonce',
        converter: converters.BufferSource,
        validator: (V) => {
          if (V.byteLength < 8) {
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

function validateMacKeyLength(parameterName, zeroError) {
  return (V) => {
    if (V === 0)
      throw lazyDOMException(`${parameterName} cannot be 0`, zeroError);

    // The Web Crypto spec allows for key lengths that are not multiples of 8. We don't.
    if (V % 8)
      throw lazyDOMException(`Unsupported ${parameterName}`, 'NotSupportedError');
  };
}

for (const { 0: name, 1: zeroError } of [['KmacKeyGenParams', 'OperationError'], ['KmacImportParams', 'DataError']]) {
  converters[name] = createDictionaryConverter(
    name, [
      dictAlgorithm,
      [
        {
          key: 'length',
          converter: (V, opts) =>
            converters['unsigned long'](V, enforceRangeOptions(opts)),
          validator: validateMacKeyLength(`${name}.length`, zeroError),
        },
      ],
    ]);
}

converters.KmacParams = createDictionaryConverter(
  'KmacParams', [
    dictAlgorithm,
    [
      {
        key: 'outputLength',
        converter: (V, opts) =>
          converters['unsigned long'](V, enforceRangeOptions(opts)),
        validator: (V, opts) => {
          // The Web Crypto spec allows for KMAC output length that are not multiples of 8. We don't.
          if (V % 8)
            throw lazyDOMException('Unsupported KmacParams outputLength', 'NotSupportedError');
        },
        required: true,
      },
      {
        key: 'customization',
        converter: converters.BufferSource,
      },
    ],
  ]);

converters.KangarooTwelveParams = createDictionaryConverter(
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
      },
    ],
  ]);

converters.TurboShakeParams = createDictionaryConverter(
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
  converters,
  requiredArguments,
};
