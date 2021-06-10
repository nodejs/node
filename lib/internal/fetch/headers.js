'use strict';

const {
  codes: { ERR_INVALID_ARG_VALUE },
} = require('internal/errors');

const {
  ObjectEntries,
  ArrayIsArray,
  StringPrototypeToLocaleLowerCase,
  StringPrototypeReplace,
  Symbol,
  SymbolIterator,
  Array,
} = primordials;

const { validateObject } = require('internal/validators');

const { isBoxedPrimitive } = require('internal/util/types');

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
  let high = arr.length / 2;

  while (high > low) {
    const mid = (high + low) >>> 1;

    if (val.localeCompare(arr[mid * 2]) > 0) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }

  return low * 2;
}

function normalizeAndValidateHeaderName(name) {
  const normalizedHeaderName = StringPrototypeToLocaleLowerCase(name);
  validateHeaderName(normalizedHeaderName);
  return normalizedHeaderName;
}

function normalizeAndValidateHeaderValue(name, value) {
  // https://fetch.spec.whatwg.org/#concept-header-value-normalize
  const normalizedHeaderValue = StringPrototypeReplace(
    value,
    /^[\n\t\r\x20]+|[\n\t\r\x20]+$/g,
    ''
  );
  validateHeaderValue(name, normalizedHeaderValue);
  return normalizedHeaderValue;
}

function fill(headers, object) {
  if (kHeadersList in object) {
    // Object is instance of Headers
    headers[kHeadersList] = new Array(...object[kHeadersList]);
  } else if (ArrayIsArray(object)) {
    // Support both 1D and 2D arrays of header entries
    if (ArrayIsArray(object[0])) {
      // Array of arrays
      for (const header of object) {
        if (header.length !== 2) {
          throw new ERR_INVALID_ARG_VALUE('init', header, 'is not of length 2');
        }
        headers.append(header[0], header[1]);
      }
    } else if (typeof object[0] === 'string' || Buffer.isBuffer(object[0])) {
      if (object.length % 2 !== 0) {
        throw new ERR_INVALID_ARG_VALUE('init', object, 'is not even length');
      }
      for (let i = 0; i < object.length; i += 2) {
        headers.append(
          object[i].toString('utf-8'),
          object[i + 1].toString('utf-8')
        );
      }
    } else {
      throw new ERR_INVALID_ARG_VALUE(
        'init',
        object,
        'is not a valid array entry'
      );
    }
  } else if (!isBoxedPrimitive(object)) {
    for (const { 0: name, 1: value } of ObjectEntries(object)) {
      headers.append(name, value);
    }
  }
}

class Headers {
  constructor(init) {
    this[kHeadersList] = [];

    if (init && validateObject(init, 'init', { allowArray: true })) {
      fill(this, init);
    }
  }

  append(name, value) {
    const normalizedName = normalizeAndValidateHeaderName(name);
    const normalizedValue = normalizeAndValidateHeaderValue(name, value);

    const index = binarySearch(this[kHeadersList], normalizedName);

    if (this[kHeadersList][index] === normalizedName) {
      this[kHeadersList][index + 1] += `, ${normalizedValue}`;
    } else {
      this[kHeadersList].splice(index, 0, normalizedName, normalizedValue);
    }
  }

  delete(name) {
    const normalizedName = normalizeAndValidateHeaderName(name);

    const index = binarySearch(this[kHeadersList], normalizedName);

    if (this[kHeadersList][index] === normalizedName) {
      this[kHeadersList].splice(index, 2);
    }
  }

  get(name) {
    const normalizedName = normalizeAndValidateHeaderName(name);

    const index = binarySearch(this[kHeadersList], normalizedName);

    if (this[kHeadersList][index] === normalizedName) {
      return this[kHeadersList][index + 1];
    }

    return null;
  }

  has(name) {
    const normalizedName = normalizeAndValidateHeaderName(name);

    const index = binarySearch(this[kHeadersList], normalizedName);

    return this[kHeadersList][index] === normalizedName;
  }

  set(name, value) {
    const normalizedName = normalizeAndValidateHeaderName(name);
    const normalizedValue = normalizeAndValidateHeaderValue(name, value);

    const index = binarySearch(this[kHeadersList], normalizedName);
    if (this[kHeadersList][index] === normalizedName) {
      this[kHeadersList][index + 1] = normalizedValue;
    } else {
      this[kHeadersList].splice(index, 2, normalizedName, normalizedValue);
    }
  }

  *keys() {
    for (const header of this) {
      yield header[0];
    }
  }

  *values() {
    for (const header of this) {
      yield header[1];
    }
  }

  *entries() {
    yield* this;
  }

  forEach(callback, thisArg) {
    for (let index = 0; index < this[kHeadersList].length; index += 2) {
      callback.call(
        thisArg,
        this[kHeadersList][index + 1],
        this[kHeadersList][index],
        this
      );
    }
  }

  *[SymbolIterator]() {
    for (let index = 0; index < this[kHeadersList].length; index += 2) {
      yield [this[kHeadersList][index], this[kHeadersList][index + 1]];
    }
  }
}

module.exports = {
  Headers,
};
