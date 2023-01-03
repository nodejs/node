'use strict';

// Adapted from the following sources
// - https://github.com/jsdom/webidl-conversions
//   Copyright Domenic Denicola. Licensed under BSD-2-Clause License.
//   Original license at https://github.com/jsdom/webidl-conversions/blob/master/LICENSE.md.
// - https://github.com/denoland/deno
//   Copyright Deno authors. Licensed under MIT License.
//   Original license at https://github.com/denoland/deno/blob/main/LICENSE.md.
// Changes include using primordials and stripping the code down to only what
// WebCryptoAPI needs.

const {
  ArrayBufferIsView,
  ArrayBufferPrototype,
  ArrayPrototypePush,
  ArrayPrototypeSort,
  DataViewPrototypeGetBuffer,
  globalThis: {
    SharedArrayBuffer,
  },
  MathPow,
  MathTrunc,
  Number,
  NumberIsFinite,
  ObjectAssign,
  ObjectCreate,
  ObjectDefineProperty,
  ObjectPrototypeIsPrototypeOf,
  ReflectHas,
  SafeArrayIterator,
  SafeSet,
  String,
  SymbolIterator,
  TypedArrayPrototypeGetSymbolToStringTag,
  TypeError,
  TypedArrayPrototypeGetBuffer,
} = primordials;

const { kEmptyObject } = require('internal/util');
const { isDataView } = require('internal/util/types');
const { CryptoKey } = require('internal/crypto/webcrypto');

function codedTypeError(message, errorProperties = kEmptyObject) {
  // eslint-disable-next-line no-restricted-syntax
  const err = new TypeError(message);
  ObjectAssign(err, errorProperties);
  return err;
}

function makeException(message, opts = kEmptyObject) {
  const prefix = opts.prefix ? opts.prefix + ': ' : '';
  const context = opts.context?.length === 0 ?
    '' : (opts.context ?? 'Value') + ' ';
  return codedTypeError(
    `${prefix}${context}${message}`,
    { code: opts.code || 'ERR_INVALID_ARG_TYPE' },
  );
}

// https://tc39.es/ecma262/#sec-tonumber
function toNumber(value, opts = kEmptyObject) {
  switch (typeof value) {
    case 'number':
      return value;
    case 'bigint':
      throw makeException(
        'is a BigInt and cannot be converted to a number.',
        opts);
    case 'symbol':
      throw makeException(
        'is a Symbol and cannot be converted to a number.',
        opts);
    default:
      return Number(value);
  }
}

function type(V) {
  if (V === null)
    return 'Null';

  switch (typeof V) {
    case 'undefined':
      return 'Undefined';
    case 'boolean':
      return 'Boolean';
    case 'number':
      return 'Number';
    case 'string':
      return 'String';
    case 'symbol':
      return 'Symbol';
    case 'bigint':
      return 'BigInt';
    case 'object': // Fall through
    case 'function': // Fall through
    default:
      // Per ES spec, typeof returns an implemention-defined value that is not
      // any of the existing ones for uncallable non-standard exotic objects.
      // Yet Type() which the Web IDL spec depends on returns Object for such
      // cases. So treat the default case as an object.
      return 'Object';
  }
}

const integerPart = MathTrunc;

// This was updated to only consider bitlength up to 32 used by WebCryptoAPI
function createIntegerConversion(bitLength) {
  const lowerBound = 0;
  const upperBound = MathPow(2, bitLength) - 1;

  const twoToTheBitLength = MathPow(2, bitLength);

  return (V, opts = kEmptyObject) => {
    let x = toNumber(V, opts);

    if (opts.enforceRange) {
      if (!NumberIsFinite(x)) {
        throw makeException(
          'is not a finite number.',
          opts);
      }

      x = integerPart(x);

      if (x < lowerBound || x > upperBound) {
        throw makeException(
          `is outside the expected range of ${lowerBound} to ${upperBound}.`,
          { ...opts, code: 'ERR_OUT_OF_RANGE' },
        );
      }

      return x;
    }

    if (!NumberIsFinite(x) || x === 0) {
      return 0;
    }

    x = integerPart(x);

    if (x >= lowerBound && x <= upperBound) {
      return x;
    }

    x = x % twoToTheBitLength;

    return x;
  };
}

const converters = {};

converters.boolean = (val) => !!val;
converters.octet = createIntegerConversion(8);
converters['unsigned short'] = createIntegerConversion(16);
converters['unsigned long'] = createIntegerConversion(32);

converters.DOMString = function(V, opts = kEmptyObject) {
  if (typeof V === 'string') {
    return V;
  } else if (typeof V === 'symbol') {
    throw makeException(
      'is a Symbol and cannot be converted to a string.',
      opts);
  }

  return String(V);
};

converters.object = (V, opts) => {
  if (type(V) !== 'Object') {
    throw makeException(
      'is not an object.',
      opts);
  }

  return V;
};

function isNonSharedArrayBuffer(V) {
  return ObjectPrototypeIsPrototypeOf(ArrayBufferPrototype, V);
}

function isSharedArrayBuffer(V) {
  return ObjectPrototypeIsPrototypeOf(SharedArrayBuffer.prototype, V);
}

function getDataViewOrTypedArrayBuffer(V) {
  return isDataView(V) ?
    DataViewPrototypeGetBuffer(V) : TypedArrayPrototypeGetBuffer(V);
}

converters.Uint8Array = (V, opts = kEmptyObject) => {
  if (!ArrayBufferIsView(V) ||
    TypedArrayPrototypeGetSymbolToStringTag(V) !== 'Uint8Array') {
    throw makeException(
      'is not an Uint8Array object.',
      opts);
  }
  if (isSharedArrayBuffer(getDataViewOrTypedArrayBuffer(V))) {
    throw makeException(
      'is a view on a SharedArrayBuffer, which is not allowed.',
      opts);
  }

  return V;
};

converters.BufferSource = (V, opts = kEmptyObject) => {
  if (ArrayBufferIsView(V)) {
    if (isSharedArrayBuffer(getDataViewOrTypedArrayBuffer(V))) {
      throw makeException(
        'is a view on a SharedArrayBuffer, which is not allowed.',
        opts);
    }

    return V;
  }

  if (!isNonSharedArrayBuffer(V)) {
    throw makeException(
      'is not instance of ArrayBuffer, Buffer, TypedArray, or DataView.',
      opts);
  }

  return V;
};

converters['sequence<DOMString>'] = createSequenceConverter(
  converters.DOMString);

function requiredArguments(length, required, opts = kEmptyObject) {
  if (length < required) {
    throw makeException(
      `${required} argument${
        required === 1 ? '' : 's'
      } required, but only ${length} present.`,
      { ...opts, context: '', code: 'ERR_MISSING_ARGS' });
  }
}

function createDictionaryConverter(name, ...dictionaries) {
  let hasRequiredKey = false;
  const allMembers = [];
  for (const members of new SafeArrayIterator(dictionaries)) {
    for (const member of new SafeArrayIterator(members)) {
      if (member.required) {
        hasRequiredKey = true;
      }
      ArrayPrototypePush(allMembers, member);
    }
  }
  ArrayPrototypeSort(allMembers, (a, b) => {
    if (a.key === b.key) {
      return 0;
    }
    return a.key < b.key ? -1 : 1;
  });

  const defaultValues = ObjectCreate(null);
  for (const member of new SafeArrayIterator(allMembers)) {
    if (ReflectHas(member, 'defaultValue')) {
      const idlMemberValue = member.defaultValue;
      const imvType = typeof idlMemberValue;
      // Copy by value types can be directly assigned, copy by reference types
      // need to be re-created for each allocation.
      if (
        imvType === 'number' ||
        imvType === 'boolean' ||
        imvType === 'string' ||
        imvType === 'bigint' ||
        imvType === 'undefined'
      ) {
        defaultValues[member.key] = member.converter(idlMemberValue, {});
      } else {
        ObjectDefineProperty(defaultValues, member.key, {
          __proto__: null,
          get() {
            return member.converter(idlMemberValue, member.defaultValue);
          },
          enumerable: true,
        });
      }
    }
  }

  return function(V, opts = kEmptyObject) {
    const typeV = type(V);
    switch (typeV) {
      case 'Undefined':
      case 'Null':
      case 'Object':
        break;
      default:
        throw makeException(
          'can not be converted to a dictionary',
          opts);
    }
    const esDict = V;

    const idlDict = ObjectAssign({}, defaultValues);

    // Fast path null and undefined.
    if (V == null && !hasRequiredKey) {
      return idlDict;
    }

    for (const member of new SafeArrayIterator(allMembers)) {
      const key = member.key;

      let esMemberValue;
      if (typeV === 'Undefined' || typeV === 'Null') {
        esMemberValue = undefined;
      } else {
        esMemberValue = esDict[key];
      }

      if (esMemberValue !== undefined) {
        const context = `'${key}' of '${name}'${
          opts.context ? ` (${opts.context})` : ''
        }`;
        const converter = member.converter;
        const idlMemberValue = converter(esMemberValue, { ...opts, context });
        idlDict[key] = idlMemberValue;
      } else if (member.required) {
        throw makeException(
          `can not be converted to '${name}' because '${key}' is required in '${name}'.`,
          { ...opts, code: 'ERR_MISSING_OPTION' });
      }
    }

    return idlDict;
  };
}

function createEnumConverter(name, values) {
  const E = new SafeSet(values);

  return function(V, opts = kEmptyObject) {
    const S = String(V);

    if (!E.has(S)) {
      throw makeException(
        `value '${S}' is not a valid enum value of type ${name}.`,
        { ...opts, code: 'ERR_INVALID_ARG_VALUE' });
    }

    return S;
  };
}

function createSequenceConverter(converter) {
  return function(V, opts = kEmptyObject) {
    if (type(V) !== 'Object') {
      throw makeException(
        'can not be converted to sequence.',
        opts);
    }
    const iter = V?.[SymbolIterator]?.();
    if (iter === undefined) {
      throw makeException(
        'can not be converted to sequence.',
        opts);
    }
    const array = [];
    while (true) {
      const res = iter?.next?.();
      if (res === undefined) {
        throw makeException(
          'can not be converted to sequence.',
          opts);
      }
      if (res.done === true) break;
      const val = converter(res.value, {
        ...opts,
        context: `${opts.context}, index ${array.length}`,
      });
      ArrayPrototypePush(array, val);
    }
    return array;
  };
}

function createInterfaceConverter(name, prototype) {
  return (V, opts) => {
    if (!ObjectPrototypeIsPrototypeOf(prototype, V)) {
      throw makeException(
        `is not of type ${name}.`,
        opts);
    }
    return V;
  };
}

converters.AlgorithmIdentifier = (V, opts) => {
  // Union for (object or DOMString)
  if (type(V) === 'Object') {
    return converters.object(V, opts);
  }
  return converters.DOMString(V, opts);
};

converters['BufferSource or JsonWebKey'] = (V, opts) => {
  // Union for (BufferSource or JsonWebKey)
  if (
    ArrayBufferIsView(V) ||
    ObjectPrototypeIsPrototypeOf(ArrayBufferPrototype, V)
  ) {
    return converters.BufferSource(V, opts);
  }
  return converters.JsonWebKey(V, opts);
};

converters.KeyFormat = createEnumConverter('KeyFormat', [
  'raw',
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
]);

converters['sequence<KeyUsage>'] = createSequenceConverter(converters.KeyUsage);

converters.HashAlgorithmIdentifier = converters.AlgorithmIdentifier;

const dictAlgorithm = [
  {
    key: 'name',
    converter: converters.DOMString,
    required: true,
  },
];

converters.Algorithm = createDictionaryConverter(
  'Algorithm', dictAlgorithm);

converters.BigInteger = converters.Uint8Array;

const dictRsaKeyGenParams = [
  ...new SafeArrayIterator(dictAlgorithm),
  {
    key: 'modulusLength',
    converter: (V, opts) =>
      converters['unsigned long'](V, { ...opts, enforceRange: true }),
    required: true,
  },
  {
    key: 'publicExponent',
    converter: converters.BigInteger,
    required: true,
  },
];

converters.RsaKeyGenParams = createDictionaryConverter(
  'RsaKeyGenParams', dictRsaKeyGenParams);

converters.RsaHashedKeyGenParams = createDictionaryConverter(
  'RsaHashedKeyGenParams', [
    ...new SafeArrayIterator(dictRsaKeyGenParams),
    {
      key: 'hash',
      converter: converters.HashAlgorithmIdentifier,
      required: true,
    },
  ]);

converters.RsaHashedImportParams = createDictionaryConverter(
  'RsaHashedImportParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'hash',
      converter: converters.HashAlgorithmIdentifier,
      required: true,
    },
  ]);

converters.NamedCurve = converters.DOMString;

converters.EcKeyImportParams = createDictionaryConverter(
  'EcKeyImportParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'namedCurve',
      converter: converters.NamedCurve,
      required: true,
    },
  ]);

converters.EcKeyGenParams = createDictionaryConverter(
  'EcKeyGenParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'namedCurve',
      converter: converters.NamedCurve,
      required: true,
    },
  ]);

converters.AesKeyGenParams = createDictionaryConverter(
  'AesKeyGenParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'length',
      converter: (V, opts) =>
        converters['unsigned short'](V, { ...opts, enforceRange: true }),
      required: true,
    },
  ]);

converters.HmacKeyGenParams = createDictionaryConverter(
  'HmacKeyGenParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'hash',
      converter: converters.HashAlgorithmIdentifier,
      required: true,
    },
    {
      key: 'length',
      converter: (V, opts) =>
        converters['unsigned long'](V, { ...opts, enforceRange: true }),
    },
  ]);

converters.RsaPssParams = createDictionaryConverter(
  'RsaPssParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'saltLength',
      converter: (V, opts) =>
        converters['unsigned long'](V, { ...opts, enforceRange: true }),
      required: true,
    },
  ]);

converters.RsaOaepParams = createDictionaryConverter(
  'RsaOaepParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'label',
      converter: converters.BufferSource,
    },
  ]);

converters.EcdsaParams = createDictionaryConverter(
  'EcdsaParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'hash',
      converter: converters.HashAlgorithmIdentifier,
      required: true,
    },
  ]);

converters.HmacImportParams = createDictionaryConverter(
  'HmacImportParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'hash',
      converter: converters.HashAlgorithmIdentifier,
      required: true,
    },
    {
      key: 'length',
      converter: (V, opts) =>
        converters['unsigned long'](V, { ...opts, enforceRange: true }),
    },
  ]);

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
  ]);

converters.HkdfParams = createDictionaryConverter(
  'HkdfParams', [
    ...new SafeArrayIterator(dictAlgorithm),
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
      required: true,
    },
  ]);

converters.Pbkdf2Params = createDictionaryConverter(
  'Pbkdf2Params', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'hash',
      converter: converters.HashAlgorithmIdentifier,
      required: true,
    },
    {
      key: 'iterations',
      converter: (V, opts) =>
        converters['unsigned long'](V, { ...opts, enforceRange: true }),
      required: true,
    },
    {
      key: 'salt',
      converter: converters.BufferSource,
      required: true,
    },
  ]);

converters.AesDerivedKeyParams = createDictionaryConverter(
  'AesDerivedKeyParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'length',
      converter: (V, opts) =>
        converters['unsigned short'](V, { ...opts, enforceRange: true }),
      required: true,
    },
  ]);

converters.AesCbcParams = createDictionaryConverter(
  'AesCbcParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'iv',
      converter: converters.BufferSource,
      required: true,
    },
  ]);

converters.AesGcmParams = createDictionaryConverter(
  'AesGcmParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'iv',
      converter: converters.BufferSource,
      required: true,
    },
    {
      key: 'tagLength',
      converter: (V, opts) =>
        converters.octet(V, { ...opts, enforceRange: true }),
    },
    {
      key: 'additionalData',
      converter: converters.BufferSource,
    },
  ]);

converters.AesCtrParams = createDictionaryConverter(
  'AesCtrParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'counter',
      converter: converters.BufferSource,
      required: true,
    },
    {
      key: 'length',
      converter: (V, opts) =>
        converters.octet(V, { ...opts, enforceRange: true }),
      required: true,
    },
  ]);

converters.CryptoKey = createInterfaceConverter(
  'CryptoKey', CryptoKey.prototype);

converters.EcdhKeyDeriveParams = createDictionaryConverter(
  'EcdhKeyDeriveParams', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'public',
      converter: converters.CryptoKey,
      required: true,
    },
  ]);

converters.Ed448Params = createDictionaryConverter(
  'Ed448Params', [
    ...new SafeArrayIterator(dictAlgorithm),
    {
      key: 'context',
      converter: converters.BufferSource,
      required: false,
    },
  ]);

module.exports = {
  converters,
  requiredArguments,
};
