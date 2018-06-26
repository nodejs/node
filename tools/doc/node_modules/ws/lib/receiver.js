'use strict';

const safeBuffer = require('safe-buffer');

const PerMessageDeflate = require('./permessage-deflate');
const bufferUtil = require('./buffer-util');
const validation = require('./validation');
const constants = require('./constants');

const Buffer = safeBuffer.Buffer;

const GET_INFO = 0;
const GET_PAYLOAD_LENGTH_16 = 1;
const GET_PAYLOAD_LENGTH_64 = 2;
const GET_MASK = 3;
const GET_DATA = 4;
const INFLATING = 5;

/**
 * HyBi Receiver implementation.
 */
class Receiver {
  /**
   * Creates a Receiver instance.
   *
   * @param {Object} extensions An object containing the negotiated extensions
   * @param {Number} maxPayload The maximum allowed message length
   * @param {String} binaryType The type for binary data
   */
  constructor (extensions, maxPayload, binaryType) {
    this._binaryType = binaryType || constants.BINARY_TYPES[0];
    this._extensions = extensions || {};
    this._maxPayload = maxPayload | 0;

    this._bufferedBytes = 0;
    this._buffers = [];

    this._compressed = false;
    this._payloadLength = 0;
    this._fragmented = 0;
    this._masked = false;
    this._fin = false;
    this._mask = null;
    this._opcode = 0;

    this._totalPayloadLength = 0;
    this._messageLength = 0;
    this._fragments = [];

    this._cleanupCallback = null;
    this._isCleaningUp = false;
    this._hadError = false;
    this._loop = false;

    this.add = this.add.bind(this);
    this.onmessage = null;
    this.onclose = null;
    this.onerror = null;
    this.onping = null;
    this.onpong = null;

    this._state = GET_INFO;
  }

  /**
   * Consumes `n` bytes from the buffered data, calls `cleanup` if necessary.
   *
   * @param {Number} n The number of bytes to consume
   * @return {(Buffer|null)} The consumed bytes or `null` if `n` bytes are not
   *     available
   * @private
   */
  consume (n) {
    if (this._bufferedBytes < n) {
      this._loop = false;
      if (this._isCleaningUp) this.cleanup(this._cleanupCallback);
      return null;
    }

    this._bufferedBytes -= n;

    if (n === this._buffers[0].length) return this._buffers.shift();

    if (n < this._buffers[0].length) {
      const buf = this._buffers[0];
      this._buffers[0] = buf.slice(n);
      return buf.slice(0, n);
    }

    const dst = Buffer.allocUnsafe(n);

    do {
      const buf = this._buffers[0];

      if (n >= buf.length) {
        this._buffers.shift().copy(dst, dst.length - n);
      } else {
        buf.copy(dst, dst.length - n, 0, n);
        this._buffers[0] = buf.slice(n);
      }

      n -= buf.length;
    } while (n > 0);

    return dst;
  }

  /**
   * Adds new data to the parser.
   *
   * @param {Buffer} chunk A chunk of data
   * @public
   */
  add (chunk) {
    this._bufferedBytes += chunk.length;
    this._buffers.push(chunk);
    this.startLoop();
  }

  /**
   * Starts the parsing loop.
   *
   * @private
   */
  startLoop () {
    this._loop = true;

    do {
      switch (this._state) {
        case GET_INFO:
          this.getInfo();
          break;
        case GET_PAYLOAD_LENGTH_16:
          this.getPayloadLength16();
          break;
        case GET_PAYLOAD_LENGTH_64:
          this.getPayloadLength64();
          break;
        case GET_MASK:
          this.getMask();
          break;
        case GET_DATA:
          this.getData();
          break;
        default: // `INFLATING`
          this._loop = false;
      }
    } while (this._loop);
  }

  /**
   * Reads the first two bytes of a frame.
   *
   * @private
   */
  getInfo () {
    const buf = this.consume(2);
    if (buf === null) return;

    if ((buf[0] & 0x30) !== 0x00) {
      this.error(
        new RangeError('Invalid WebSocket frame: RSV2 and RSV3 must be clear'),
        1002
      );
      return;
    }

    const compressed = (buf[0] & 0x40) === 0x40;

    if (compressed && !this._extensions[PerMessageDeflate.extensionName]) {
      this.error(
        new RangeError('Invalid WebSocket frame: RSV1 must be clear'),
        1002
      );
      return;
    }

    this._fin = (buf[0] & 0x80) === 0x80;
    this._opcode = buf[0] & 0x0f;
    this._payloadLength = buf[1] & 0x7f;

    if (this._opcode === 0x00) {
      if (compressed) {
        this.error(
          new RangeError('Invalid WebSocket frame: RSV1 must be clear'),
          1002
        );
        return;
      }

      if (!this._fragmented) {
        this.error(
          new RangeError('Invalid WebSocket frame: invalid opcode 0'),
          1002
        );
        return;
      } else {
        this._opcode = this._fragmented;
      }
    } else if (this._opcode === 0x01 || this._opcode === 0x02) {
      if (this._fragmented) {
        this.error(
          new RangeError(
            `Invalid WebSocket frame: invalid opcode ${this._opcode}`
          ),
          1002
        );
        return;
      }

      this._compressed = compressed;
    } else if (this._opcode > 0x07 && this._opcode < 0x0b) {
      if (!this._fin) {
        this.error(
          new RangeError('Invalid WebSocket frame: FIN must be set'),
          1002
        );
        return;
      }

      if (compressed) {
        this.error(
          new RangeError('Invalid WebSocket frame: RSV1 must be clear'),
          1002
        );
        return;
      }

      if (this._payloadLength > 0x7d) {
        this.error(
          new RangeError(
            `Invalid WebSocket frame: invalid payload length ` +
              `${this._payloadLength}`
          ),
          1002
        );
        return;
      }
    } else {
      this.error(
        new RangeError(
          `Invalid WebSocket frame: invalid opcode ${this._opcode}`
        ),
        1002
      );
      return;
    }

    if (!this._fin && !this._fragmented) this._fragmented = this._opcode;

    this._masked = (buf[1] & 0x80) === 0x80;

    if (this._payloadLength === 126) this._state = GET_PAYLOAD_LENGTH_16;
    else if (this._payloadLength === 127) this._state = GET_PAYLOAD_LENGTH_64;
    else this.haveLength();
  }

  /**
   * Gets extended payload length (7+16).
   *
   * @private
   */
  getPayloadLength16 () {
    const buf = this.consume(2);
    if (buf === null) return;

    this._payloadLength = buf.readUInt16BE(0, true);
    this.haveLength();
  }

  /**
   * Gets extended payload length (7+64).
   *
   * @private
   */
  getPayloadLength64 () {
    const buf = this.consume(8);
    if (buf === null) return;

    const num = buf.readUInt32BE(0, true);

    //
    // The maximum safe integer in JavaScript is 2^53 - 1. An error is returned
    // if payload length is greater than this number.
    //
    if (num > Math.pow(2, 53 - 32) - 1) {
      this.error(
        new RangeError(
          'Unsupported WebSocket frame: payload length > 2^53 - 1'
        ),
        1009
      );
      return;
    }

    this._payloadLength = num * Math.pow(2, 32) + buf.readUInt32BE(4, true);
    this.haveLength();
  }

  /**
   * Payload length has been read.
   *
   * @private
   */
  haveLength () {
    if (this._opcode < 0x08 && this.maxPayloadExceeded(this._payloadLength)) {
      return;
    }

    if (this._masked) this._state = GET_MASK;
    else this._state = GET_DATA;
  }

  /**
   * Reads mask bytes.
   *
   * @private
   */
  getMask () {
    this._mask = this.consume(4);
    if (this._mask === null) return;

    this._state = GET_DATA;
  }

  /**
   * Reads data bytes.
   *
   * @private
   */
  getData () {
    var data = constants.EMPTY_BUFFER;

    if (this._payloadLength) {
      data = this.consume(this._payloadLength);
      if (data === null) return;

      if (this._masked) bufferUtil.unmask(data, this._mask);
    }

    if (this._opcode > 0x07) {
      this.controlMessage(data);
    } else if (this._compressed) {
      this._state = INFLATING;
      this.decompress(data);
    } else if (this.pushFragment(data)) {
      this.dataMessage();
    }
  }

  /**
   * Decompresses data.
   *
   * @param {Buffer} data Compressed data
   * @private
   */
  decompress (data) {
    const perMessageDeflate = this._extensions[PerMessageDeflate.extensionName];

    perMessageDeflate.decompress(data, this._fin, (err, buf) => {
      if (err) {
        this.error(err, err.closeCode === 1009 ? 1009 : 1007);
        return;
      }

      if (this.pushFragment(buf)) this.dataMessage();
      this.startLoop();
    });
  }

  /**
   * Handles a data message.
   *
   * @private
   */
  dataMessage () {
    if (this._fin) {
      const messageLength = this._messageLength;
      const fragments = this._fragments;

      this._totalPayloadLength = 0;
      this._messageLength = 0;
      this._fragmented = 0;
      this._fragments = [];

      if (this._opcode === 2) {
        var data;

        if (this._binaryType === 'nodebuffer') {
          data = toBuffer(fragments, messageLength);
        } else if (this._binaryType === 'arraybuffer') {
          data = toArrayBuffer(toBuffer(fragments, messageLength));
        } else {
          data = fragments;
        }

        this.onmessage(data);
      } else {
        const buf = toBuffer(fragments, messageLength);

        if (!validation.isValidUTF8(buf)) {
          this.error(
            new Error('Invalid WebSocket frame: invalid UTF-8 sequence'),
            1007
          );
          return;
        }

        this.onmessage(buf.toString());
      }
    }

    this._state = GET_INFO;
  }

  /**
   * Handles a control message.
   *
   * @param {Buffer} data Data to handle
   * @private
   */
  controlMessage (data) {
    if (this._opcode === 0x08) {
      if (data.length === 0) {
        this._loop = false;
        this.onclose(1005, '');
        this.cleanup(this._cleanupCallback);
      } else if (data.length === 1) {
        this.error(
          new RangeError('Invalid WebSocket frame: invalid payload length 1'),
          1002
        );
      } else {
        const code = data.readUInt16BE(0, true);

        if (!validation.isValidStatusCode(code)) {
          this.error(
            new RangeError(
              `Invalid WebSocket frame: invalid status code ${code}`
            ),
            1002
          );
          return;
        }

        const buf = data.slice(2);

        if (!validation.isValidUTF8(buf)) {
          this.error(
            new Error('Invalid WebSocket frame: invalid UTF-8 sequence'),
            1007
          );
          return;
        }

        this._loop = false;
        this.onclose(code, buf.toString());
        this.cleanup(this._cleanupCallback);
      }

      return;
    }

    if (this._opcode === 0x09) this.onping(data);
    else this.onpong(data);

    this._state = GET_INFO;
  }

  /**
   * Handles an error.
   *
   * @param {Error} err The error
   * @param {Number} code Close code
   * @private
   */
  error (err, code) {
    this._hadError = true;
    this._loop = false;
    this.onerror(err, code);
    this.cleanup(this._cleanupCallback);
  }

  /**
   * Checks payload size, disconnects socket when it exceeds `maxPayload`.
   *
   * @param {Number} length Payload length
   * @private
   */
  maxPayloadExceeded (length) {
    if (length === 0 || this._maxPayload < 1) return false;

    const fullLength = this._totalPayloadLength + length;

    if (fullLength <= this._maxPayload) {
      this._totalPayloadLength = fullLength;
      return false;
    }

    this.error(new RangeError('Max payload size exceeded'), 1009);
    return true;
  }

  /**
   * Appends a fragment in the fragments array after checking that the sum of
   * fragment lengths does not exceed `maxPayload`.
   *
   * @param {Buffer} fragment The fragment to add
   * @return {Boolean} `true` if `maxPayload` is not exceeded, else `false`
   * @private
   */
  pushFragment (fragment) {
    if (fragment.length === 0) return true;

    const totalLength = this._messageLength + fragment.length;

    if (this._maxPayload < 1 || totalLength <= this._maxPayload) {
      this._messageLength = totalLength;
      this._fragments.push(fragment);
      return true;
    }

    this.error(new RangeError('Max payload size exceeded'), 1009);
    return false;
  }

  /**
   * Releases resources used by the receiver.
   *
   * @param {Function} cb Callback
   * @public
   */
  cleanup (cb) {
    if (this._extensions === null) {
      if (cb) cb();
      return;
    }

    if (!this._hadError && (this._loop || this._state === INFLATING)) {
      this._cleanupCallback = cb;
      this._isCleaningUp = true;
      return;
    }

    this._extensions = null;
    this._fragments = null;
    this._buffers = null;
    this._mask = null;

    this._cleanupCallback = null;
    this.onmessage = null;
    this.onclose = null;
    this.onerror = null;
    this.onping = null;
    this.onpong = null;

    if (cb) cb();
  }
}

module.exports = Receiver;

/**
 * Makes a buffer from a list of fragments.
 *
 * @param {Buffer[]} fragments The list of fragments composing the message
 * @param {Number} messageLength The length of the message
 * @return {Buffer}
 * @private
 */
function toBuffer (fragments, messageLength) {
  if (fragments.length === 1) return fragments[0];
  if (fragments.length > 1) return bufferUtil.concat(fragments, messageLength);
  return constants.EMPTY_BUFFER;
}

/**
 * Converts a buffer to an `ArrayBuffer`.
 *
 * @param {Buffer} The buffer to convert
 * @return {ArrayBuffer} Converted buffer
 */
function toArrayBuffer (buf) {
  if (buf.byteOffset === 0 && buf.byteLength === buf.buffer.byteLength) {
    return buf.buffer;
  }

  return buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength);
}
