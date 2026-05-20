'use strict';

const {
  ArrayBufferIsView,
  ArrayBufferPrototypeGetResizable,
  ArrayIsArray,
  ArrayPrototypePush,
  ArrayPrototypeToSorted,
  BigInt,
  DataViewPrototypeGetBuffer,
  FunctionPrototypeCall,
  MathAbs,
  MathMax,
  MathMin,
  MathPow,
  MathSign,
  MathTrunc,
  Number,
  NumberIsFinite,
  NumberIsNaN,
  NumberMAX_SAFE_INTEGER,
  NumberMIN_SAFE_INTEGER,
  ObjectPrototypeIsPrototypeOf,
  SafeArrayIterator,
  SafeSet,
  String,
  SymbolIterator,
  TypeError,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetSymbolToStringTag,
} = primordials;

const { kEmptyObject, setOwnProperty } = require('internal/util');
const {
  isSharedArrayBuffer,
  isTypedArray,
} = require('internal/util/types');

const BIGINT_2_63 = 1n << 63n;
const BIGINT_2_64 = 1n << 64n;

const converters = { __proto__: null };

/**
 * @typedef {object} ConversionOptions
 * @property {string} [prefix] Message prefix for operation failures.
 * @property {string} [context] Message context for the converted value.
 * @property {string} [code] Node.js error code to assign to TypeError.
 * @property {boolean} [enforceRange] Web IDL [EnforceRange] attribute.
 * @property {boolean} [clamp] Web IDL [Clamp] attribute.
 * @property {boolean} [allowShared] Web IDL [AllowShared] attribute for
 *   buffer view types.
 * @property {boolean} [allowResizable] Web IDL [AllowResizable] attribute.
 */

/**
 * @callback Converter
 * @param {any} V JavaScript value to convert to an IDL value.
 * @param {ConversionOptions} [options] Conversion options.
 * @returns {any}
 */

/**
 * @callback DictionaryMemberValidator
 * @param {any} idlMemberValue Converted IDL member value.
 * @param {object} jsDict Original JavaScript dictionary object.
 * @returns {void}
 */

/**
 * @typedef {object} DictionaryMember
 * @property {string} key Dictionary member identifier.
 * @property {Converter} converter Converter for the member type.
 * @property {boolean} [required] Whether the member is required.
 * @property {() => any} [defaultValue] Function returning the default value.
 * @property {DictionaryMemberValidator} [validator] Optional Node.js
 *   extension point invoked after conversion and before storing the member.
 *   This is for early semantic validation of known unsupported IDL values,
 *   especially in Web Crypto, where SubtleCrypto.supports() needs to answer
 *   from normalized dictionaries without running the requested operation.
 */

/**
 * Creates a TypeError with a Node.js error code.
 * @param {string} message Error message.
 * @param {string} code Node.js error code to assign.
 * @returns {TypeError}
 */
function codedTypeError(message, code) {
  // eslint-disable-next-line no-restricted-syntax
  const err = new TypeError(message);
  setOwnProperty(err, 'code', code);
  return err;
}

/**
 * Creates the exception thrown by Web IDL converters.
 * @param {string} message Unprefixed conversion failure message.
 * @param {ConversionOptions} [options] Conversion options.
 * @returns {TypeError}
 */
function makeException(message, options = kEmptyObject) {
  const prefix = options.prefix ? options.prefix + ': ' : '';
  const context = options.context?.length === 0 ?
    '' : (options.context ?? 'Value') + ' ';
  return codedTypeError(
    `${prefix}${context}${message}`,
    options.code || 'ERR_INVALID_ARG_TYPE',
  );
}

/**
 * Builds derived conversion options for nested converter calls and adjusted
 * error codes. These objects are allocated on dictionary/sequence conversion
 * hot paths, so keep their shape stable and avoid object spread and
 * null-prototype objects.
 * @param {ConversionOptions} options Parent conversion options.
 * @param {string} [context] Replacement context.
 * @param {string} [code] Replacement error code.
 * @returns {ConversionOptions}
 */
function makeOptions(options, context = options.context, code = options.code) {
  return {
    prefix: options.prefix,
    context,
    code,
    enforceRange: options.enforceRange,
    clamp: options.clamp,
    allowShared: options.allowShared,
    allowResizable: options.allowResizable,
  };
}

/**
 * Returns the ECMAScript specification type of a JavaScript value.
 * @see https://tc39.es/ecma262/#sec-ecmascript-data-types-and-values
 * @param {any} V JavaScript value.
 * @returns {'Undefined'|'Null'|'Boolean'|'String'|'Symbol'|'Number'|'BigInt'|'Object'}
 */
function type(V) {
  // ECMA-262 6.1: map JavaScript values to language type names.
  switch (typeof V) {
    case 'undefined':
      return 'Undefined';
    case 'boolean':
      return 'Boolean';
    case 'string':
      return 'String';
    case 'symbol':
      return 'Symbol';
    case 'number':
      return 'Number';
    case 'bigint':
      return 'BigInt';
    case 'object':
    case 'function':
    default:
      // ECMA-262 6.1.2: null is its own language type.
      // ECMA-262 6.1.7: functions are Object values.
      if (V === null) {
        return 'Null';
      }
      return 'Object';
  }
}

/**
 * Returns IntegerPart(n).
 * @see https://webidl.spec.whatwg.org/#abstract-opdef-integerpart
 * @param {number} n Numeric value.
 * @returns {number}
 */
function integerPart(n) {
  // Web IDL IntegerPart steps 1-3: floor(abs(n)), restore the sign,
  // and choose +0 rather than -0.
  const integer = MathTrunc(n);
  return integer === 0 ? 0 : integer;
}

/**
 * Rounds to the nearest integer, choosing the even integer on ties.
 * @param {number} x Numeric value.
 * @returns {number}
 */
function evenRound(x) {
  // Web IDL ConvertToInt step 7.2: round to the nearest integer,
  // choosing the even integer on ties and +0 rather than -0.
  const i = integerPart(x);
  const remainder = MathAbs(x % 1);
  const sign = MathSign(x);
  if (remainder === 0.5) {
    return i % 2 === 0 ? i : i + sign;
  }
  const r = remainder < 0.5 ? i : i + sign;
  if (r === 0) {
    return 0;
  }
  return r;
}

/**
 * Returns 2 to the power of the given exponent.
 * @param {number} exponent Non-negative integer exponent.
 * @returns {number}
 */
function pow2(exponent) {
  if (exponent < 31) {
    return 1 << exponent;
  }
  if (exponent === 31) {
    return 0x8000_0000;
  }
  if (exponent === 32) {
    return 0x1_0000_0000;
  }
  return MathPow(2, exponent);
}

/**
 * Returns x modulo y for Web IDL ConvertToInt step 10.
 *
 * This is intentionally not a general modulo helper. ConvertToInt only calls
 * it with a positive power-of-two modulus, and the implementation assumes
 * that. It converts JavaScript remainder into mathematical modulo and
 * normalizes -0 to +0.
 * @param {number} x Dividend.
 * @param {number} y Positive divisor.
 * @returns {number}
 */
function modulo(x, y) {
  // Web IDL ConvertToInt step 10 uses mathematical modulo.
  const r = x % y;
  if (r === 0) {
    return 0;
  }
  return r > 0 ? r : r + y;
}

/**
 * Returns x modulo y for Web IDL ConvertToInt step 10.
 *
 * This is intentionally not a general modulo helper. ConvertToInt only calls
 * it with a positive power-of-two modulus, and the implementation assumes
 * that. BigInt has no -0, but this mirrors modulo()'s mathematical modulo
 * behavior for the 64-bit path.
 * @param {bigint} x Dividend.
 * @param {bigint} y Positive divisor.
 * @returns {bigint}
 */
function bigIntModulo(x, y) {
  // Web IDL ConvertToInt step 10 uses mathematical modulo.
  const r = x % y;
  return r >= 0n ? r : r + y;
}

/**
 * Returns ToNumber(V).
 * @see https://tc39.es/ecma262/#sec-tonumber
 * @param {any} V JavaScript value.
 * @param {ConversionOptions} [options] Conversion options.
 * @returns {number}
 */
function toNumber(V, options = kEmptyObject) {
  if (typeof V === 'bigint') {
    // ECMA-262 ToNumber step 2: BigInt values throw.
    throw makeException(
      'is a BigInt and cannot be converted to a number.',
      options);
  }
  if (typeof V === 'symbol') {
    // ECMA-262 ToNumber step 2: Symbol values throw.
    throw makeException(
      'is a Symbol and cannot be converted to a number.',
      options);
  }

  // Unary plus performs ToNumber, including ToPrimitive(V, number) for
  // objects. Number(V) is not equivalent because it converts BigInt values,
  // including BigInt values produced by ToPrimitive.
  // Abrupt completions and native TypeErrors propagate unchanged. This is an
  // intentional diagnostics tradeoff: decorating object conversion failures
  // would require maintaining local ECMA-262 ToPrimitive and
  // OrdinaryToPrimitive implementations.
  return +V;
}

/**
 * Returns ToString(V).
 * @see https://tc39.es/ecma262/#sec-tostring
 * @param {any} V JavaScript value.
 * @param {ConversionOptions} [options] Conversion options.
 * @returns {string}
 */
function toString(V, options = kEmptyObject) {
  if (typeof V === 'symbol') {
    // ECMA-262 ToString step 2: Symbol values throw.
    throw makeException(
      'is a Symbol and cannot be converted to a string.',
      options);
  }

  // The String function performs ToString for all non-Symbol primitives and
  // objects, including ToPrimitive(V, string). String concatenation is not
  // equivalent because it uses ToPrimitive(V, default). Abrupt completions
  // and native TypeErrors propagate unchanged. This is an intentional
  // diagnostics tradeoff: decorating object conversion failures would require
  // maintaining local ECMA-262 ToPrimitive and OrdinaryToPrimitive
  // implementations.
  return String(V);
}

/**
 * Converts a JavaScript value to a Web IDL integer value.
 * @see https://webidl.spec.whatwg.org/#abstract-opdef-converttoint
 * @param {any} V JavaScript value.
 * @param {number} bitLength Integer bit length.
 * @param {'signed'|'unsigned'} [signedness] Integer signedness.
 * @param {ConversionOptions} [options] Conversion options.
 * @returns {number}
 */
function convertToInt(
  V,
  bitLength,
  signedness = 'unsigned',
  options = kEmptyObject,
) {
  const signed = signedness === 'signed';
  let upperBound;
  let lowerBound;

  // Web IDL ConvertToInt steps 1-3: determine lower/upper bounds.
  if (bitLength === 64) {
    // Steps 1.1-1.3 set upperBound to 2^53 - 1 and lowerBound to 0
    // for unsigned, or -2^53 + 1 for signed. This ensures 64-bit
    // integer types associated with [EnforceRange] or [Clamp] are
    // representable in JavaScript's Number type as unambiguous integers.
    upperBound = NumberMAX_SAFE_INTEGER;
    lowerBound = signed ? NumberMIN_SAFE_INTEGER : 0;
  } else if (!signed) {
    // Spell out the common Web IDL integer sizes so hot converters avoid
    // recomputing powers of two on every call.
    lowerBound = 0;
    if (bitLength === 8) {
      upperBound = 0xff;
    } else if (bitLength === 16) {
      upperBound = 0xffff;
    } else if (bitLength === 32) {
      upperBound = 0xffff_ffff;
    } else {
      upperBound = pow2(bitLength) - 1;
    }
  } else if (bitLength === 8) {
    // Signed 8/16/32-bit conversions are mostly exercised through direct
    // convertToInt() calls, but keep their common bounds cheap too.
    lowerBound = -0x80;
    upperBound = 0x7f;
  } else if (bitLength === 16) {
    lowerBound = -0x8000;
    upperBound = 0x7fff;
  } else if (bitLength === 32) {
    lowerBound = -0x8000_0000;
    upperBound = 0x7fff_ffff;
  } else {
    lowerBound = -pow2(bitLength - 1);
    upperBound = pow2(bitLength - 1) - 1;
  }

  // Common case: primitive Number values that already fit the Web IDL
  // range and have no fractional part are returned unchanged by every
  // ConvertToInt path, except that -0 must become +0. This skips the
  // generic ToNumber and option handling without skipping observable
  // object coercion.
  let x;
  if (typeof V === 'number') {
    // For primitive Numbers, in-range non-[Clamp] conversion is either
    // identity or IntegerPart(V). This keeps the default and [EnforceRange]
    // paths out of the generic ToNumber/options flow.
    if (V >= lowerBound && V <= upperBound) {
      const integer = MathTrunc(V);
      if (integer === V) {
        return V === 0 ? 0 : V;
      }
      if (options !== kEmptyObject && options.clamp) {
        return evenRound(V);
      }
      return integer === 0 ? 0 : integer;
    }
    if (options !== kEmptyObject && options.enforceRange) {
      // Keep [EnforceRange] ahead of [Clamp] without falling through to
      // the shared check, which would observe options.enforceRange again.
      if (!NumberIsFinite(V)) {
        throw makeException(
          'is not a finite number.',
          options);
      }

      const integer = integerPart(V);
      if (integer < lowerBound || integer > upperBound) {
        throw makeException(
          `is outside the expected range of ${lowerBound} to ${upperBound}.`,
          makeOptions(options, options.context, 'ERR_OUT_OF_RANGE'));
      }

      return integer;
    }
    if (options !== kEmptyObject && options.clamp && !NumberIsNaN(V)) {
      // Out-of-range [Clamp] returns one of the already-computed bounds.
      if (V <= lowerBound) {
        return lowerBound === 0 ? 0 : lowerBound;
      }
      if (V >= upperBound) {
        return upperBound === 0 ? 0 : upperBound;
      }
    }
    x = V;
  } else {
    // Step 4: convert V with ECMA-262 ToNumber.
    x = toNumber(V, options);
  }
  // Step 5: normalize -0 to +0.
  if (x === 0) {
    x = 0;
  }

  // Step 6: [EnforceRange] rejects non-finite and out-of-range values.
  if (options.enforceRange) {
    // Step 6.1: reject NaN and infinities.
    if (!NumberIsFinite(x)) {
      throw makeException(
        'is not a finite number.',
        options);
    }

    // Step 6.2: truncate to IntegerPart(x).
    x = integerPart(x);

    // Steps 6.3-6.4: reject out-of-range values, otherwise return.
    if (x < lowerBound || x > upperBound) {
      throw makeException(
        `is outside the expected range of ${lowerBound} to ${upperBound}.`,
        makeOptions(options, options.context, 'ERR_OUT_OF_RANGE'));
    }

    return x;
  }

  // Step 7: [Clamp] clamps, rounds, and returns non-NaN values.
  if (options.clamp && !NumberIsNaN(x)) {
    // Step 7.1: clamp x into the supported bounds.
    x = MathMin(MathMax(x, lowerBound), upperBound);
    // Steps 7.2-7.3: round ties to even and return.
    return evenRound(x);
  }

  // Step 8: NaN, +0, -0, and infinities become +0.
  if (!NumberIsFinite(x) || x === 0) {
    return 0;
  }

  // Step 9: truncate to IntegerPart(x).
  x = integerPart(x);

  // Steps 10-12 are an identity for values already in the step 1-3
  // bounds. For 64-bit conversions this only skips the safe-integer
  // subset; values outside it still need exact BigInt modulo and the
  // final Number approximation.
  if (x >= lowerBound && x <= upperBound) {
    return x;
  }

  if (bitLength === 64) {
    // Steps 10-12 still wrap over the full 2^64 IDL integer range.
    // BigInt keeps x modulo 2^64 and the signed high-bit adjustment exact
    // before this helper returns the JavaScript binding result.
    let xBigInt = BigInt(x);
    xBigInt = bigIntModulo(xBigInt, BIGINT_2_64);

    // For long long and unsigned long long values outside the safe-integer
    // range, Web IDL says the JS Number value represents the closest numeric
    // value, choosing the value with an even significand if there are two
    // equally close values. Number(BigInt) performs that final approximation.

    // Step 11: wrap into the signed range when the high bit is set.
    if (signed && xBigInt >= BIGINT_2_63) {
      return Number(xBigInt - BIGINT_2_64);
    }

    // Step 12: return the unsigned value.
    return Number(xBigInt);
  }

  // For 8/16/32-bit conversions, bitwise operators perform the same
  // power-of-two wrapping as Web IDL step 10 for finite integer Numbers.
  // The shifts narrow the unsigned value into the signed range when needed.
  if (bitLength === 8) {
    return signed ? (x << 24) >> 24 : x & 0xff;
  }
  if (bitLength === 16) {
    return signed ? (x << 16) >> 16 : x & 0xffff;
  }
  if (bitLength === 32) {
    return signed ? x | 0 : x >>> 0;
  }

  // Step 10: reduce modulo 2^bitLength.
  const twoToTheBitLength = pow2(bitLength);
  x = modulo(x, twoToTheBitLength);

  // Step 11: wrap into the signed range when the high bit is set.
  if (signed && x >= pow2(bitLength - 1)) {
    return x - twoToTheBitLength;
  }

  // Step 12: return the unsigned value.
  return x;
}

/**
 * Creates a converter for a Web IDL integer type.
 * @param {number} bitLength Integer bit length.
 * @param {'signed'|'unsigned'} [signedness] Integer signedness.
 * @returns {Converter}
 */
function createIntegerConverter(bitLength, signedness = 'unsigned') {
  return (V, options = kEmptyObject) => {
    // Integer conversion step 1 calls ConvertToInt; step 2 returns
    // the IDL value with the same numeric value.
    return convertToInt(V, bitLength, signedness, options);
  };
}

/**
 * Converts a JavaScript value to the IDL boolean type.
 * @see https://webidl.spec.whatwg.org/#es-boolean
 * @param {any} V JavaScript value.
 * @returns {boolean}
 */
converters.boolean = (V) => {
  // Web IDL boolean steps 1-2: ToBoolean(V), then return the same
  // truth value as an IDL boolean.
  return !!V;
};

/**
 * Converts a JavaScript value to the IDL object type.
 * @see https://webidl.spec.whatwg.org/#es-object
 * @param {any} V JavaScript value.
 * @param {ConversionOptions} [options] Conversion options.
 * @returns {object|Function}
 */
converters.object = (V, options = kEmptyObject) => {
  // Web IDL object step 1: throw unless V is an ECMA-262 Object.
  if (type(V) !== 'Object') {
    throw makeException(
      'is not an object.',
      options,
    );
  }
  // Step 2: return a reference to the same object.
  return V;
};

/**
 * Converts a JavaScript value to the IDL octet type.
 * @see https://webidl.spec.whatwg.org/#es-octet
 * @type {Converter}
 */
converters.octet = createIntegerConverter(8);

/**
 * Converts a JavaScript value to the IDL unsigned short type.
 * @see https://webidl.spec.whatwg.org/#es-unsigned-short
 * @type {Converter}
 */
converters['unsigned short'] = createIntegerConverter(16);

/**
 * Converts a JavaScript value to the IDL unsigned long type.
 * @see https://webidl.spec.whatwg.org/#es-unsigned-long
 * @type {Converter}
 */
converters['unsigned long'] = createIntegerConverter(32);

/**
 * Converts a JavaScript value to the IDL long long type.
 * @see https://webidl.spec.whatwg.org/#es-long-long
 * @type {Converter}
 */
converters['long long'] = createIntegerConverter(64, 'signed');

/**
 * Converts a JavaScript value to the IDL DOMString type.
 * @see https://webidl.spec.whatwg.org/#es-DOMString
 * @param {any} V JavaScript value.
 * @param {ConversionOptions} [options] Conversion options.
 * @returns {string}
 */
converters.DOMString = function DOMString(V, options = kEmptyObject) {
  // Step 1 only applies to [LegacyNullToEmptyString], which this core
  // converter does not implement. Steps 2-3 apply ToString(V) and
  // return a DOMString with the same code units.
  return toString(V, options);
};

/**
 * Throws when a Web IDL operation receives too few arguments.
 * @param {number} length Actual argument count.
 * @param {number} required Required argument count.
 * @param {ConversionOptions} [options] Conversion options.
 * @returns {void}
 */
function requiredArguments(length, required, options = kEmptyObject) {
  if (length < required) {
    throw makeException(
      `${required} argument${
        required === 1 ? '' : 's'
      } required, but only ${length} present.`,
      makeOptions(options, '', 'ERR_MISSING_ARGS'));
  }
}

/**
 * Creates a converter for a Web IDL enum type.
 * @see https://webidl.spec.whatwg.org/#es-enumeration
 * @param {string} name Enum identifier.
 * @param {string[]} values Enum values.
 * @returns {Converter}
 */
function createEnumConverter(name, values) {
  const E = new SafeSet(new SafeArrayIterator(values));

  return function(V, options = kEmptyObject) {
    // Web IDL enumeration step 1: convert V with ToString.
    const S = toString(V, options);

    // Step 2: throw unless S is one of the enumeration values.
    if (!E.has(S)) {
      throw makeException(
        `'${S}' is not a valid enum value of type ${name}.`,
        makeOptions(options, options.context, 'ERR_INVALID_ARG_VALUE'));
    }

    // Step 3: return the matching enumeration value.
    return S;
  };
}

/**
 * Returns the context used when converting a dictionary member.
 * @param {string} key Dictionary member identifier.
 * @param {ConversionOptions} options Conversion options.
 * @returns {string}
 */
function dictionaryMemberContext(key, options) {
  return options.context ? `${key} in ${options.context}` : key;
}

/**
 * Returns the context used when converting a sequence element.
 * @param {number} index Sequence element index.
 * @param {ConversionOptions} options Conversion options.
 * @returns {string}
 */
function sequenceElementContext(index, options) {
  return `${options.context ?? 'Value'}[${index}]`;
}

/**
 * Returns the message used for a missing required dictionary member.
 * @param {string} dictionaryName Dictionary identifier.
 * @param {string} key Dictionary member identifier.
 * @returns {string}
 */
function missingDictionaryMemberMessage(dictionaryName, key) {
  return `cannot be converted to '${dictionaryName}' because ` +
    `'${key}' is required in '${dictionaryName}'.`;
}

/**
 * Creates a converter for a Web IDL dictionary type.
 * @see https://webidl.spec.whatwg.org/#js-dictionary
 * @param {string} dictionaryName Dictionary identifier.
 * @param {DictionaryMember[]|DictionaryMember[][]} members Dictionary members,
 *   either for a single dictionary or grouped from least-derived to
 *   most-derived dictionary.
 * @returns {Converter}
 */
function createDictionaryConverter(
  dictionaryName,
  members,
) {
  const compareMembers = (a, b) => {
    if (a.key === b.key) {
      return 0;
    }
    return a.key < b.key ? -1 : 1;
  };

  const dictionaries = ArrayIsArray(members[0]) ? members : [members];
  const sortedDictionaries = [];

  // Web IDL dictionary conversion steps 3-4 process inherited dictionaries
  // from least-derived to most-derived and sort only within each dictionary.
  // Callers with inheritance pass one member array per dictionary level.
  for (let i = 0; i < dictionaries.length; i++) {
    ArrayPrototypePush(
      sortedDictionaries,
      ArrayPrototypeToSorted(dictionaries[i], compareMembers),
    );
  }

  return function(jsDict, options = kEmptyObject) {
    // Step 1: reject non-object, non-null, non-undefined values.
    if (jsDict != null && type(jsDict) !== 'Object') {
      throw makeException(
        'cannot be converted to a dictionary',
        options,
      );
    }

    // Step 2: create the IDL dictionary value.
    const idlDict = { __proto__: null };

    // Steps 3-4: iterate each dictionary level, then its sorted members.
    for (let i = 0; i < sortedDictionaries.length; i++) {
      const sortedMembers = sortedDictionaries[i];
      for (let j = 0; j < sortedMembers.length; j++) {
        const member = sortedMembers[j];
        // Step 4.1.1: get the dictionary member identifier.
        const key = member.key;
        // Steps 4.1.2-4.1.3: read the JavaScript member value.
        const jsMemberValue = jsDict == null ? undefined : jsDict[key];

        // Step 4.1.4: convert and store present member values.
        if (jsMemberValue !== undefined) {
          const converter = member.converter;
          // Step 4.1.4.1: convert the JavaScript value to IDL.
          const idlMemberValue = converter(
            jsMemberValue,
            makeOptions(options, dictionaryMemberContext(key, options)),
          );
          // Validators are a Node.js extension after conversion. They let
          // consumers reject known unsupported values while dictionary
          // conversion still has precise member context. Web Crypto uses this
          // so SubtleCrypto.supports() can make accurate decisions from
          // normalized dictionaries instead of probing by running operations.
          member.validator?.(idlMemberValue, jsDict);
          // Step 4.1.4.2: set idlDict[key] to the IDL value.
          idlDict[key] = idlMemberValue;
        } else if (typeof member.defaultValue === 'function') {
          // Step 4.1.5: store the member default value.
          idlDict[key] = member.defaultValue();
        } else if (member.required) {
          // Step 4.1.6: required missing members throw.
          throw makeException(
            missingDictionaryMemberMessage(dictionaryName, key),
            makeOptions(options, options.context, 'ERR_MISSING_OPTION'));
        }
      }
    }

    // Step 5: return the IDL dictionary.
    return idlDict;
  };
}

/**
 * Creates a converter for a Web IDL sequence type.
 * @see https://webidl.spec.whatwg.org/#es-sequence
 * @param {Converter} converter Element converter.
 * @returns {Converter}
 */
function createSequenceConverter(converter) {
  return function(V, options = kEmptyObject) {
    // Web IDL sequence conversion step 1: require an ECMA-262 Object.
    if (type(V) !== 'Object') {
      throw makeException(
        'cannot be converted to sequence.',
        options);
    }

    // Step 2: GetMethod(V, %Symbol.iterator%).
    const method = V[SymbolIterator];
    // Step 3: throw if the iterator method is undefined, null, or not callable.
    if (typeof method !== 'function') {
      throw makeException(
        'cannot be converted to sequence.',
        options);
    }

    // Step 4 and create-sequence step 1: get the iterator record.
    const iterator = FunctionPrototypeCall(method, V);
    const nextMethod = iterator?.next;
    if (typeof nextMethod !== 'function') {
      throw makeException(
        'cannot be converted to sequence.',
        options);
    }

    // Create-sequence step 2: initialize i to 0.
    const idlSequence = [];
    while (true) {
      // Step 3.1: IteratorStepValue(iteratorRecord).
      const next = FunctionPrototypeCall(nextMethod, iterator);
      if (type(next) !== 'Object') {
        throw makeException(
          'cannot be converted to sequence.',
          options);
      }
      // Step 3.2: IteratorComplete applies ToBoolean(done).
      if (next.done) {
        break;
      }
      // Step 3.3: convert next to an IDL value of type T.
      const idlValue = converter(
        next.value,
        makeOptions(options, sequenceElementContext(idlSequence.length, options)),
      );
      // Step 3.4: store the value and advance i.
      ArrayPrototypePush(idlSequence, idlValue);
    }
    return idlSequence;
  };
}

/**
 * Creates a converter for a Web IDL interface type.
 * @see https://webidl.spec.whatwg.org/#js-interface
 * @param {string} name Interface identifier.
 * @param {object} prototype Interface prototype object.
 * @returns {Converter}
 */
function createInterfaceConverter(name, prototype) {
  return (V, options = kEmptyObject) => {
    // Web IDL interface conversion step 1: return V if it implements I.
    if (ObjectPrototypeIsPrototypeOf(prototype, V)) {
      return V;
    }
    // Step 2: otherwise throw.
    throw makeException(
      `is not of type ${name}.`,
      options);
  };
}

/**
 * Returns the ArrayBuffer or SharedArrayBuffer viewed by a typed array or
 * DataView.
 * @param {ArrayBufferView} V TypedArray or DataView.
 * @returns {ArrayBuffer|SharedArrayBuffer}
 */
function getViewedArrayBuffer(V) {
  // Buffer view conversion steps read V.[[ViewedArrayBuffer]].
  return isTypedArray(V) ?
    TypedArrayPrototypeGetBuffer(V) : DataViewPrototypeGetBuffer(V);
}

/**
 * Validates [AllowShared] and [AllowResizable] backing-store constraints.
 * @param {ArrayBuffer|SharedArrayBuffer} buffer Backing buffer.
 * @param {ConversionOptions} options Conversion options.
 * @returns {void}
 */
function validateBufferSourceBacking(buffer, options) {
  let resizable;
  try {
    // ArrayBuffer.prototype.resizable is an ArrayBuffer brand check and the
    // [AllowResizable] value we need. For SharedArrayBuffer-backed views it
    // throws, which lets this path avoid a separate IsSharedArrayBuffer check.
    // BufferSource has separate inline logic because [AllowShared] cannot be
    // used with that typedef.
    resizable = ArrayBufferPrototypeGetResizable(buffer);
  } catch {
    // ArrayBufferView conversion step 2: reject SharedArrayBuffer
    // backing stores unless [AllowShared] is present.
    if (!options.allowShared) {
      throw makeException(
        'is a view on a SharedArrayBuffer, which is not allowed.',
        options);
    }
    // Step 3: reject non-fixed SharedArrayBuffer backing stores unless
    // [AllowResizable] is present.
    validateAllowGrowableSharedArrayBuffer(buffer, options);
    return;
  }

  // ArrayBuffer conversion step 3 and ArrayBufferView conversion step 3:
  // reject non-fixed ArrayBuffer backing stores unless [AllowResizable]
  // is present.
  validateAllowResizableArrayBuffer(resizable, options);
}

/**
 * Validates the [AllowResizable] constraint for growable SharedArrayBuffer.
 * @param {SharedArrayBuffer} buffer SharedArrayBuffer backing buffer.
 * @param {ConversionOptions} options Conversion options.
 * @returns {void}
 */
function validateAllowGrowableSharedArrayBuffer(buffer, options) {
  // SharedArrayBuffer and ArrayBufferView conversion step 3:
  // IsFixedLengthArrayBuffer(buffer) must be true without [AllowResizable].
  // Do not use a primordial getter here. When this module is included in the
  // startup snapshot, an early-captured SharedArrayBuffer.prototype.growable
  // getter does not detect growable buffers created after deserialization.
  // Lazily capturing the getter would work, but it would observe the runtime
  // prototype at first comparison, so it would not be an actual primordial.
  if (!options.allowResizable && buffer.growable) {
    throw makeException(
      'is backed by a growable SharedArrayBuffer, which is not allowed.',
      options);
  }
}

/**
 * Validates the [AllowResizable] constraint for resizable ArrayBuffer.
 * @param {boolean} resizable ArrayBuffer [[ArrayBufferResizable]] value.
 * @param {ConversionOptions} options Conversion options.
 * @returns {void}
 */
function validateAllowResizableArrayBuffer(resizable, options) {
  // ArrayBuffer and ArrayBufferView conversion step 3:
  // IsFixedLengthArrayBuffer(buffer) must be true without [AllowResizable].
  // Read [[ArrayBufferResizable]] first so fixed buffers skip the options
  // property lookup on this hot path.
  if (resizable && !options.allowResizable) {
    throw makeException(
      'is backed by a resizable ArrayBuffer, which is not allowed.',
      options);
  }
}

/**
 * Converts a JavaScript value to the IDL Uint8Array type.
 * @param {any} V JavaScript value.
 * @param {ConversionOptions} [options] Conversion options.
 * @returns {Uint8Array}
 */
converters.Uint8Array = (V, options = kEmptyObject) => {
  // Typed array conversion steps 1-2: T is Uint8Array, and V must
  // have [[TypedArrayName]] equal to "Uint8Array".
  if (!ArrayBufferIsView(V) ||
    TypedArrayPrototypeGetSymbolToStringTag(V) !== 'Uint8Array') {
    throw makeException(
      'is not an Uint8Array object.',
      options);
  }
  // Steps 3-4: validate [AllowShared] and [AllowResizable].
  validateBufferSourceBacking(TypedArrayPrototypeGetBuffer(V), options);

  // Step 5: return a reference to the same object.
  return V;
};

/**
 * Converts a JavaScript value to the IDL BufferSource typedef.
 * @see https://webidl.spec.whatwg.org/#BufferSource
 * @param {any} V JavaScript value.
 * @param {ConversionOptions} [options] Conversion options.
 * @returns {ArrayBuffer|ArrayBufferView<ArrayBuffer>}
 */
converters.BufferSource = (V, options = kEmptyObject) => {
  // BufferSource is a typedef for (ArrayBufferView or ArrayBuffer).
  // [AllowShared] cannot be used with BufferSource because the ArrayBuffer
  // union branch does not support it. Use AllowSharedBufferSource instead.
  if (ArrayBufferIsView(V)) {
    const buffer = getViewedArrayBuffer(V);
    // ArrayBufferView conversion steps 2-4: validate the viewed buffer
    // and return a reference to the same view.
    // Keep this logic inline instead of calling validateBufferSourceBacking().
    // BufferSource conversion is hot, and this avoids a helper call while still
    // using a primordial getter for the backing-buffer internal-slot check.
    // Unlike validateBufferSourceBacking(), this intentionally ignores
    // options.allowShared because [AllowShared] cannot be used with
    // BufferSource.
    let resizable;
    try {
      // ArrayBuffer.prototype.resizable is both the ArrayBuffer brand check
      // and the step 3 value. It throws for SharedArrayBuffer, which lets this
      // path reject SAB-backed views without an extra byteLength getter call.
      resizable = ArrayBufferPrototypeGetResizable(buffer);
    } catch {
      throw makeException(
        'is a view on a SharedArrayBuffer, which is not allowed.',
        options);
    }
    if (resizable && !options.allowResizable) {
      throw makeException(
        'is backed by a resizable ArrayBuffer, which is not allowed.',
        options);
    }
    return V;
  }

  // ArrayBuffer conversion steps 1-2: require a non-shared ArrayBuffer.
  // Use the primordial resizable getter as both the ArrayBuffer brand check
  // and the step 3 value. This avoids isArrayBuffer(V) followed by another
  // getter call, and rejects SharedArrayBuffer on this union branch.
  let resizable;
  try {
    resizable = ArrayBufferPrototypeGetResizable(V);
  } catch {
    throw makeException(
      'is not instance of ArrayBuffer, Buffer, TypedArray, or DataView.',
      options);
  }

  // ArrayBuffer conversion step 3: validate [AllowResizable].
  // ArrayBufferPrototypeGetResizable(V) already excluded SharedArrayBuffer,
  // so no [AllowShared] validation is needed on this branch.
  if (resizable && !options.allowResizable) {
    throw makeException(
      'is backed by a resizable ArrayBuffer, which is not allowed.',
      options);
  }

  // Step 4: return a reference to the same ArrayBuffer.
  return V;
};

/**
 * Converts a JavaScript value to the IDL AllowSharedBufferSource typedef.
 * @see https://webidl.spec.whatwg.org/#AllowSharedBufferSource
 * @param {any} V JavaScript value.
 * @param {ConversionOptions} [options] Conversion options.
 * @returns {ArrayBuffer|SharedArrayBuffer|ArrayBufferView}
 */
converters.AllowSharedBufferSource = (V, options = kEmptyObject) => {
  // AllowSharedBufferSource is a typedef for
  // (ArrayBuffer or SharedArrayBuffer or [AllowShared] ArrayBufferView).
  // The union branches are disjoint, so this keeps the hot view path first.
  if (ArrayBufferIsView(V)) {
    const buffer = getViewedArrayBuffer(V);
    let resizable;
    try {
      resizable = ArrayBufferPrototypeGetResizable(buffer);
    } catch {
      validateAllowGrowableSharedArrayBuffer(buffer, options);
      return V;
    }
    validateAllowResizableArrayBuffer(resizable, options);
    return V;
  }

  let resizable;
  try {
    resizable = ArrayBufferPrototypeGetResizable(V);
  } catch {
    if (isSharedArrayBuffer(V)) {
      validateAllowGrowableSharedArrayBuffer(V, options);
      return V;
    }
    throw makeException(
      'is not instance of ArrayBuffer, SharedArrayBuffer, Buffer, ' +
      'TypedArray, or DataView.',
      options);
  }

  validateAllowResizableArrayBuffer(resizable, options);
  return V;
};

/**
 * Converts a JavaScript value to the IDL sequence<DOMString> type.
 * @see https://webidl.spec.whatwg.org/#es-sequence
 * @type {Converter}
 */
converters['sequence<DOMString>'] =
  createSequenceConverter(converters.DOMString);

/**
 * Converts a JavaScript value to the IDL sequence<object> type.
 * @see https://webidl.spec.whatwg.org/#es-sequence
 * @type {Converter}
 */
converters['sequence<object>'] =
  createSequenceConverter(converters.object);

module.exports = {
  converters,
  convertToInt,
  createDictionaryConverter,
  createEnumConverter,
  createInterfaceConverter,
  createSequenceConverter,
  requiredArguments,
  type,
};
