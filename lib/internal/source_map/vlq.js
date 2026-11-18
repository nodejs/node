'use strict';

const {
  Int8Array,
  StringPrototypeCharAt,
  StringPrototypeCharCodeAt,
  TypedArrayPrototypeFill,
} = primordials;

// The base64 digit value of each char code below 128, or -1.
const kDigitValues = TypedArrayPrototypeFill(new Int8Array(128), -1);
{
  const digits =
    'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  for (let i = 0; i < digits.length; ++i) {
    kDigitValues[StringPrototypeCharCodeAt(digits, i)] = i;
  }
}

class StringCharIterator {
  /**
   * @param {string} string
   */
  constructor(string) {
    this._string = string;
    this._position = 0;
  }

  /**
   * @returns {string}
   */
  next() {
    return StringPrototypeCharAt(this._string, this._position++);
  }

  /**
   * @returns {string}
   */
  peek() {
    return StringPrototypeCharAt(this._string, this._position);
  }

  /**
   * @returns {boolean}
   */
  hasNext() {
    return this._position < this._string.length;
  }
}

/**
 * VLQUnsignedValue ( ): an non-negative integer
 * @see https://tc39.es/ecma426/#sec-VLQUnsignedValue
 * @param {StringCharIterator} iterator positioned at a Vlq, and moved past it
 * @returns {number} the VLQUnsignedValue of the Vlq, or -1 if the iterator
 *   isn't at a Vlq or the value is out of range
 */
function decodeUnsignedVLQ(iterator) {
  const string = iterator._string;
  let position = iterator._position;
  let result = 0;
  let shift = 0;
  let digit;
  do {
    // Past the end of the string, the char code is NaN.
    const code = StringPrototypeCharCodeAt(string, position++);
    digit = code < 128 ? kDigitValues[code] : -1;
    if (digit === -1) {
      iterator._position = position;
      return -1;
    }
    const bits = digit & 0x1f;
    // Vlq :: VlqDigitList
    // 2. If _value_ is >= 2<sup>32</sup>, throw an error.
    if (shift >= 30 && (shift > 30 || bits > 3)) {
      iterator._position = position;
      return -1;
    }
    // VlqDigitList :: ContinuationDigit VlqDigitList
    // 3. Return _left_ + _right_ * 2<sup>5</sup>.
    result |= bits << shift;
    shift += 5;
  } while (digit & 0x20);
  iterator._position = position;
  return result >>> 0;
}

/**
 * VLQSignedValue ( ): an integer
 * @see https://tc39.es/ecma426/#sec-VLQSignedValue
 * @param {number} unsigned the VLQUnsignedValue of the VlqDigitList
 * @returns {number}
 */
function toSigned(unsigned) {
  // 2. If _unsigned_ modulo 2 = 1, let _sign_ be -1.
  // 3. Else, let _sign_ be 1.
  // 4. Let _value_ be floor(_unsigned_ / 2).
  const value = unsigned >>> 1;
  if ((unsigned & 1) === 0) return value;
  // 5. If _value_ is 0 and _sign_ is -1, return -2<sup>31</sup>.
  // 7. Return _sign_ * _value_.
  return value === 0 ? -2147483648 : -value;
}

/**
 * VLQSignedValue ( ): an integer
 * @see https://tc39.es/ecma426/#sec-VLQSignedValue
 * @param {StringCharIterator} iterator positioned at a Vlq, and moved past it
 * @returns {number} the VLQSignedValue of the Vlq, or 0 if the iterator isn't
 *   at a Vlq or the value is out of range
 */
function decodeSignedVLQ(iterator) {
  const unsigned = decodeUnsignedVLQ(iterator);
  return unsigned === -1 ? 0 : toSigned(unsigned);
}

module.exports = {
  StringCharIterator,
  decodeSignedVLQ,
  decodeUnsignedVLQ,
  toSigned,
};
