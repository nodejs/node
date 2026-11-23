// Copyright 2022 Datadog, Inc.
// This product includes software developed at Datadog
// (https://www.datadoghq.com/).
//
// Modified for inclusion in Node.js core: ported from the encoding half of
// https://github.com/DataDog/pprof-format/blob/53548398f38b16eb096cc04a75927316a68a9b54/src/protobuf.ts
//
// SPDX-License-Identifier: MIT

'use strict';

// Protobuf wire-format encoding helpers for the pprof messages.

const {
  ArrayPrototypePush,
  BigIntAsUintN,
  Number,
  Promise,
  Symbol,
} = primordials;

const assert = require('internal/assert');
const { FastBuffer } = require('internal/buffer');
const { setImmediate } = require('timers');

const lowMax = 2 ** 32 - 1;
const lowMaxPlus1 = lowMax + 1;

const kTypeVarInt = 0;
const kTypeLengthDelim = 2;

// The methods that messages implement to be encoded, as fields of other
// messages or by `Message.prototype.encode()`. They are keyed by symbols so
// that they are not part of the public API of `pprof.Profile`.
const kMeasure = Symbol('kMeasure');
const kEncodeToBuffer = Symbol('kEncodeToBuffer');

/**
 * Splits a number or bigint into the high and low 32 bits of its two's
 * complement 64-bit representation.
 * @param {number|bigint} number The integer to split. Numbers must be safe
 *   integers.
 * @returns {number[]} The high and low 32 bits, as unsigned integers.
 */
function long(number) {
  if (typeof number === 'bigint') {
    return [
      Number(BigIntAsUintN(32, number >> 32n)),
      Number(BigIntAsUintN(32, number)),
    ];
  }

  const sign = number < 0;
  if (sign) {
    number = -number;
  }

  let lo = (number >>> 0) % lowMaxPlus1;
  let hi = (number / lowMaxPlus1) >>> 0;

  if (sign) {
    hi = ~hi >>> 0;
    lo = ~lo >>> 0;
    if (++lo > lowMax) {
      lo = 0;
      if (++hi > lowMax) {
        hi = 0;
      }
    }
  }

  return [hi, lo];
}

/**
 * Computes the tag of a field: its number and wire type, as encoded before
 * its value.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @param {number} wireType The wire type of the field's value, such as
 *   `kTypeVarInt` or `kTypeLengthDelim`.
 * @returns {number} The tag, before its varint encoding.
 */
function fieldTag(field, wireType) {
  return field * 8 + wireType;
}

/**
 * Measures the encoded tag of a field.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @returns {number} The number of bytes that the tag takes.
 */
function measureTag(field) {
  // Tags of fields 1 to 15 fit in one byte.
  return field < 16 ? 1 : measureNumber(fieldTag(field, 0));
}

/**
 * Encodes the tag of a field.
 * @param {Buffer} buffer The buffer to write the tag to.
 * @param {number} offset The offset at which to write the tag.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @param {number} wireType The wire type of the field's value, such as
 *   `kTypeVarInt` or `kTypeLengthDelim`.
 * @returns {number} The offset after the tag.
 */
function encodeTag(buffer, offset, field, wireType) {
  if (field < 16) {
    buffer[offset++] = fieldTag(field, wireType);
    return offset;
  }
  return encodeNumber(buffer, offset, fieldTag(field, wireType));
}

/**
 * Measures the varint encoding of a number, so that fields with a default
 * value can be omitted.
 * @param {number|bigint} number The integer to measure.
 * @returns {number} The number of bytes that the encoding takes, or `0` for
 *   `0`.
 */
function measureNumber(number) {
  let lo;
  if (typeof number === 'number' && number >= 0 && number <= lowMax) {
    if (number === 0) {
      return 0;
    }
    lo = number;
  } else {
    if (number === 0n) {
      return 0;
    }
    const words = long(number);
    const hi = words[0];
    lo = words[1];
    if (hi !== 0) {
      const c = hi >>> 24;
      if (c !== 0) {
        return c < 128 ? 9 : 10;
      }
      const b = (lo >>> 28 | hi << 4) >>> 0;
      if (b < 16384) {
        return b < 128 ? 5 : 6;
      }
      return b < 2097152 ? 7 : 8;
    }
  }

  if (lo < 16384) {
    return lo < 128 ? 1 : 2;
  }
  if (lo < 268435456) {
    return lo < 2097152 ? 3 : 4;
  }
  return 5;
}

/**
 * Like `measureNumber()`, but counts the one byte that a `0` takes when it is
 * encoded anyway, such as an empty string's length or a packed array element.
 * @param {number|bigint} number The integer to measure.
 * @returns {number} The number of bytes that the encoding takes.
 */
function varintLength(number) {
  return measureNumber(number) || 1;
}

/**
 * Encodes a number as a varint. Negative numbers are encoded as their two's
 * complement 64-bit representation.
 * @param {Buffer} buffer The buffer to write the varint to.
 * @param {number} i The offset at which to write the varint.
 * @param {number|bigint} number The integer to encode.
 * @returns {number} The offset after the varint.
 */
function encodeNumber(buffer, i, number) {
  let lo = number;
  if (typeof number !== 'number' || number < 0 || number > lowMax) {
    const words = long(number);
    let hi = words[0];
    lo = words[1];
    while (hi) {
      buffer[i++] = lo & 127 | 128;
      lo = (lo >>> 7 | hi << 25) >>> 0;
      hi >>>= 7;
    }
  }

  lo >>>= 0;
  while (lo > 127) {
    buffer[i++] = lo & 127 | 128;
    lo >>>= 7;
  }
  buffer[i++] = lo;

  return i;
}

/**
 * The payload lengths of one encoding, recorded in length-prefix order while
 * measuring and read back sequentially while encoding, to avoid measuring
 * twice. No state is stored on the messages.
 * @typedef {object} EncodeContext
 * @property {number[]} lengths The payload lengths, in length-prefix order.
 * @property {number} cursor The index in `lengths` of the next length to
 *   encode.
 */

/**
 * Creates the context of a new encoding.
 * @returns {EncodeContext} A context with no recorded lengths.
 */
function newEncodeContext() {
  return { lengths: [], cursor: 0 };
}

/**
 * Waits for the next iteration of the event loop, so that a long encoding
 * does not block other work.
 * @returns {Promise<void>} A promise that resolves in the check phase of the
 *   event loop.
 */
function yieldToEventLoop() {
  return new Promise((resolve) => setImmediate(resolve));
}

/**
 * Checks that an encoding wrote as many bytes as were measured, which fails
 * only because of a bug in the encoder or a mutation of the message while it
 * was being encoded.
 * @param {number} end The offset at which the encoding ended.
 * @param {number} length The measured length.
 * @throws {Error} An `ERR_INTERNAL_ASSERTION` error if the lengths differ.
 */
function verifyEncodedLength(end, length) {
  assert(end === length, `Encoded ${end} bytes but measured ${length}`);
}

/**
 * The base class of the messages. Subclasses implement `[kMeasure](context)`,
 * which returns the length of the message without its tag and length, and
 * `[kEncodeToBuffer](buffer, offset, context)`, which writes the message
 * without its tag and length and returns the offset after it. Messages must
 * not be mutated while they are being encoded.
 */
class Message {
  /**
   * Encodes the message.
   * @returns {Buffer} The uncompressed protobuf encoding of the message.
   * @throws {Error} An `ERR_INTERNAL_ASSERTION` error if the message was
   *   mutated while it was being encoded.
   */
  encode() {
    const context = newEncodeContext();
    const length = this[kMeasure](context);
    const buffer = new FastBuffer(length);
    verifyEncodedLength(this[kEncodeToBuffer](buffer, 0, context), length);
    return buffer;
  }
}

// Field helpers take a field number. Singular fields omit default values, and
// message fields are omitted only if undefined. Repeated fields encode every
// element, defaults included, so that indices are preserved; only an empty
// array is omitted.

/**
 * Measures a varint field, which is omitted if it is `0`.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @param {number|bigint} number The value of the field.
 * @returns {number} The number of bytes that the field takes.
 */
function measureNumberField(field, number) {
  const length = measureNumber(number);
  return length ? measureTag(field) + length : 0;
}

/**
 * Encodes a varint field, unless it is `0`.
 * @param {Buffer} buffer The buffer to write the field to.
 * @param {number} offset The offset at which to write the field.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @param {number|bigint} number The value of the field.
 * @returns {number} The offset after the field.
 */
function encodeNumberField(buffer, offset, field, number) {
  if (!number) return offset;
  offset = encodeTag(buffer, offset, field, kTypeVarInt);
  return encodeNumber(buffer, offset, number);
}

/**
 * Measures a bool field, which is omitted if it is `false`.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @param {boolean} value The value of the field.
 * @returns {number} The number of bytes that the field takes.
 */
function measureBoolField(field, value) {
  return value ? measureTag(field) + 1 : 0;
}

/**
 * Encodes a bool field, unless it is `false`.
 * @param {Buffer} buffer The buffer to write the field to.
 * @param {number} offset The offset at which to write the field.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @param {boolean} value The value of the field.
 * @returns {number} The offset after the field.
 */
function encodeBoolField(buffer, offset, field, value) {
  if (!value) return offset;
  offset = encodeTag(buffer, offset, field, kTypeVarInt);
  buffer[offset++] = 1;
  return offset;
}

/**
 * Measures a packed repeated varint field, which is omitted if it is empty,
 * and records its payload length in the context.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @param {Array<number|bigint>} values The elements of the field.
 * @param {EncodeContext} context The context of the encoding, which the
 *   payload length is recorded in.
 * @returns {number} The number of bytes that the field takes.
 */
function measurePackedNumbersField(field, values, context) {
  // Packed arrays are encoded as tag, length, concatenated elements.
  let total = 0;
  for (let i = 0; i < values.length; i++) {
    total += varintLength(values[i]);
  }
  if (!total) {
    return 0;
  }
  ArrayPrototypePush(context.lengths, total);
  return measureTag(field) + measureNumber(total) + total;
}

/**
 * Encodes a packed repeated varint field, unless it is empty.
 * @param {Buffer} buffer The buffer to write the field to.
 * @param {number} offset The offset at which to write the field.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @param {Array<number|bigint>} values The elements of the field.
 * @param {EncodeContext} context The context of the encoding, which the
 *   payload length is read from.
 * @returns {number} The offset after the field.
 */
function encodePackedNumbersField(buffer, offset, field, values, context) {
  if (!values.length) {
    return offset;
  }
  offset = encodeTag(buffer, offset, field, kTypeLengthDelim);
  offset = encodeNumber(buffer, offset, context.lengths[context.cursor++]);
  for (let i = 0; i < values.length; i++) {
    offset = encodeNumber(buffer, offset, values[i]);
  }
  return offset;
}

/**
 * Measures a message field, which is omitted if it is `undefined`, and
 * records its payload length in the context.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @param {Message|undefined} message The value of the field.
 * @param {EncodeContext} context The context of the encoding, which the
 *   payload lengths of the message and of its fields are recorded in.
 * @returns {number} The number of bytes that the field takes.
 */
function measureMessageField(field, message, context) {
  if (message === undefined) return 0;
  // Reserve the parent's slot first, to match the length-prefix order.
  const slot = ArrayPrototypePush(context.lengths, 0) - 1;
  const length = message[kMeasure](context);
  context.lengths[slot] = length;
  return measureTag(field) + varintLength(length) + length;
}

/**
 * Encodes a message field, unless it is `undefined`.
 * @param {Buffer} buffer The buffer to write the field to.
 * @param {number} offset The offset at which to write the field.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @param {Message|undefined} message The value of the field.
 * @param {EncodeContext} context The context of the encoding, which the
 *   payload lengths of the message and of its fields are read from.
 * @returns {number} The offset after the field.
 */
function encodeMessageField(buffer, offset, field, message, context) {
  if (message === undefined) return offset;
  offset = encodeTag(buffer, offset, field, kTypeLengthDelim);
  offset = encodeNumber(buffer, offset, context.lengths[context.cursor++]);
  return message[kEncodeToBuffer](buffer, offset, context);
}

/**
 * Measures a repeated message field.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @param {Message[]} messages The elements of the field.
 * @param {EncodeContext} context The context of the encoding, which the
 *   payload lengths of the elements are recorded in.
 * @returns {number} The number of bytes that the elements take.
 */
function measureMessageArrayField(field, messages, context) {
  let total = 0;
  for (let i = 0; i < messages.length; i++) {
    total += measureMessageField(field, messages[i], context);
  }
  return total;
}

/**
 * Encodes a repeated message field.
 * @param {Buffer} buffer The buffer to write the field to.
 * @param {number} offset The offset at which to write the field.
 * @param {number} field The field number, as declared in the `.proto` file.
 * @param {Message[]} messages The elements of the field.
 * @param {EncodeContext} context The context of the encoding, which the
 *   payload lengths of the elements are read from.
 * @returns {number} The offset after the elements.
 */
function encodeMessageArrayField(buffer, offset, field, messages, context) {
  for (let i = 0; i < messages.length; i++) {
    offset = encodeMessageField(buffer, offset, field, messages[i], context);
  }
  return offset;
}

module.exports = {
  Message,
  encodeBoolField,
  encodeMessageArrayField,
  encodeMessageField,
  encodeNumber,
  encodeNumberField,
  encodePackedNumbersField,
  encodeTag,
  kEncodeToBuffer,
  kMeasure,
  kTypeLengthDelim,
  measureBoolField,
  measureMessageArrayField,
  measureMessageField,
  measureNumber,
  measureNumberField,
  measurePackedNumbersField,
  measureTag,
  newEncodeContext,
  varintLength,
  verifyEncodedLength,
  yieldToEventLoop,
};
