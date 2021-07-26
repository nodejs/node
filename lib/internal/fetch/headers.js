'use strict';

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_HTTP_TOKEN,
    ERR_INVALID_THIS,
    ERR_HTTP_INVALID_HEADER_VALUE,
  },
} = require('internal/errors');

const {
  ArrayIsArray,
  ArrayPrototypeSlice,
  ArrayPrototypeSplice,
  FunctionPrototypeCall,
  MathFloor,
  ObjectDefineProperties,
  ObjectEntries,
  RegExpPrototypeSymbolReplace,
  StringPrototypeLocaleCompare,
  StringPrototypeToLocaleLowerCase,
  Symbol,
  SymbolIterator,
} = primordials;

const { validateObject } = require('internal/validators');
const { isBoxedPrimitive } = require('internal/util/types');
const {
  customInspectSymbol,
  kEnumerableProperty,
} = require('internal/util')

const { validateHeaderName, validateHeaderValue } = require('_http_outgoing');

const { Buffer } = require('buffer');

const kHeadersList = Symbol('headers list');

/**
 * This algorithm is based off of
 * https://www.tbray.org/ongoing/When/200x/2003/03/22/Binary
 * It only operates on the even indexes of the array (the header names) by only
 * iterating at most half the length of the input array. The search also
 * assumes all entries are strings and uses String.prototype.localeCompare for
 * comparison.
 */
function binarySearch(arr, val) {
  let low = 0;
  let high = MathFloor(arr.length / 2);

  while (high > low) {
    const mid = (high + low) >>> 1;

    if (StringPrototypeLocaleCompare(val, arr[mid * 2]) > 0) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }

  return low * 2;
}

function normalizeAndValidateHeaderName(name) {
  if (name === undefined) {
    throw new ERR_INVALID_HTTP_TOKEN('Header name', name);
  }
  const normalizedHeaderName = StringPrototypeToLocaleLowerCase(name);
  validateHeaderName(normalizedHeaderName);
  return normalizedHeaderName;
}

function normalizeAndValidateHeaderValue(name, value) {
  if (value === undefined) {
    throw new ERR_HTTP_INVALID_HEADER_VALUE(value, name);
  }
  // https://fetch.spec.whatwg.org/#concept-header-value-normalize
  const normalizedHeaderValue = RegExpPrototypeSymbolReplace(
    /^[\n\t\r\x20]+|[\n\t\r\x20]+$/g,
    value,
    ''
  );
  validateHeaderValue(name, normalizedHeaderValue);
  return normalizedHeaderValue;
}

function isHeaders(object) {
  return kHeadersList in object;
}

function fill(headers, object) {
  if (isHeaders(object)) {
    // Object is instance of Headers
    headers[kHeadersList] = ArrayPrototypeSlice(object[kHeadersList]);
  } else if (ArrayIsArray(object)) {
    // Support both 1D and 2D arrays of header entries
    if (ArrayIsArray(object[0])) {
      // Array of arrays
      for (let i = 0; i < object.length; i++) {
        if (object[i].length !== 2) {
          throw new ERR_INVALID_ARG_VALUE('init', object[i],
                                          'is not of length 2');
        }
        headers.append(object[i][0], object[i][1]);
      }
    } else if (typeof object[0] === 'string' || Buffer.isBuffer(object[0])) {
      // Flat array of strings or Buffers
      if (object.length % 2 !== 0) {
        throw new ERR_INVALID_ARG_VALUE('init', object,
                                        'is not even in length');
      }
      for (let i = 0; i < object.length; i += 2) {
        headers.append(
          object[i].toString('utf-8'),
          object[i + 1].toString('utf-8')
        );
      }
    } else {
      // All other array based entries
      throw new ERR_INVALID_ARG_VALUE(
        'init',
        object,
        'is not a valid array entry'
      );
    }
  } else if (!isBoxedPrimitive(object)) {
    // Object of key/value entries
    const entries = ObjectEntries(object);
    for (let i = 0; i < entries.length; i++) {
      headers.append(entries[i][0], entries[i][1]);
    }
  }
}

class Headers {
  constructor(init = {}) {
    validateObject(init, 'init', { allowArray: true });
    this[kHeadersList] = [];
    fill(this, init);
  }

  append(name, value) {
    if (!isHeaders(this)) {
      throw new ERR_INVALID_THIS('Headers');
    }

    const normalizedName = normalizeAndValidateHeaderName(name);
    const normalizedValue = normalizeAndValidateHeaderValue(name, value);

    const index = binarySearch(this[kHeadersList], normalizedName);

    if (this[kHeadersList][index] === normalizedName) {
      this[kHeadersList][index + 1] += `, ${normalizedValue}`;
    } else {
      ArrayPrototypeSplice(
        this[kHeadersList], index, 0, normalizedName, normalizedValue);
    }
  }

  delete(name) {
    if (!isHeaders(this)) {
      throw new ERR_INVALID_THIS('Headers');
    }

    const normalizedName = normalizeAndValidateHeaderName(name);

    const index = binarySearch(this[kHeadersList], normalizedName);

    if (this[kHeadersList][index] === normalizedName) {
      ArrayPrototypeSplice(this[kHeadersList], index, 2);
    }
  }

  get(name) {
    if (!isHeaders(this)) {
      throw new ERR_INVALID_THIS('Headers');
    }

    const normalizedName = normalizeAndValidateHeaderName(name);

    const index = binarySearch(this[kHeadersList], normalizedName);

    if (this[kHeadersList][index] === normalizedName) {
      return this[kHeadersList][index + 1];
    }

    return null;
  }

  has(name) {
    if (!isHeaders(this)) {
      throw new ERR_INVALID_THIS('Headers');
    }

    const normalizedName = normalizeAndValidateHeaderName(name);

    const index = binarySearch(this[kHeadersList], normalizedName);

    return this[kHeadersList][index] === normalizedName;
  }

  set(name, value) {
    if (!isHeaders(this)) {
      throw new ERR_INVALID_THIS('Headers');
    }

    const normalizedName = normalizeAndValidateHeaderName(name);
    const normalizedValue = normalizeAndValidateHeaderValue(name, value);

    const index = binarySearch(this[kHeadersList], normalizedName);
    if (this[kHeadersList][index] === normalizedName) {
      this[kHeadersList][index + 1] = normalizedValue;
    } else {
      ArrayPrototypeSplice(
        this[kHeadersList], index, 0, normalizedName, normalizedValue);
    }
  }

  * keys() {
    if (!isHeaders(this)) {
      throw new ERR_INVALID_THIS('Headers');
    }

    for (let index = 0; index < this[kHeadersList].length; index += 2) {
      yield this[kHeadersList][index];
    }
  }

  * values() {
    if (!isHeaders(this)) {
      throw new ERR_INVALID_THIS('Headers');
    }

    for (let index = 1; index < this[kHeadersList].length; index += 2) {
      yield this[kHeadersList][index];
    }
  }

  * entries() {
    if (!isHeaders(this)) {
      throw new ERR_INVALID_THIS('Headers');
    }

    for (let index = 0; index < this[kHeadersList].length; index += 2) {
      yield [this[kHeadersList][index], this[kHeadersList][index + 1]];
    }
  }

  forEach(callback, thisArg) {
    if (!isHeaders(this)) {
      throw new ERR_INVALID_THIS('Headers');
    }

    for (let index = 0; index < this[kHeadersList].length; index += 2) {
      FunctionPrototypeCall(
        callback,
        thisArg,
        this[kHeadersList][index + 1],
        this[kHeadersList][index],
        this
      );
    }
  }

  [customInspectSymbol] () {
    return this[kHeadersList]
  }
}

Headers.prototype[SymbolIterator] = Headers.prototype.entries;

ObjectDefineProperties(Headers.prototype, {
  append: kEnumerableProperty,
  delete: kEnumerableProperty,
  get: kEnumerableProperty,
  has: kEnumerableProperty,
  set: kEnumerableProperty,
  keys: kEnumerableProperty,
  values: kEnumerableProperty,
  entries: kEnumerableProperty,
  forEach: kEnumerableProperty,
});

module.exports = {
  Headers,
  binarySearch,
  kHeadersList,
};
