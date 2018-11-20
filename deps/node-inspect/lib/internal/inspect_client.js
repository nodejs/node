/*
 * Copyright Node.js contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
'use strict';
const Buffer = require('buffer').Buffer;
const crypto = require('crypto');
const { EventEmitter } = require('events');
const http = require('http');
const URL = require('url');
const util = require('util');

const debuglog = util.debuglog('inspect');

const kOpCodeText = 0x1;
const kOpCodeClose = 0x8;

const kFinalBit = 0x80;
const kReserved1Bit = 0x40;
const kReserved2Bit = 0x20;
const kReserved3Bit = 0x10;
const kOpCodeMask = 0xF;
const kMaskBit = 0x80;
const kPayloadLengthMask = 0x7F;

const kMaxSingleBytePayloadLength = 125;
const kMaxTwoBytePayloadLength = 0xFFFF;
const kTwoBytePayloadLengthField = 126;
const kEightBytePayloadLengthField = 127;
const kMaskingKeyWidthInBytes = 4;

function isEmpty(obj) {
  return Object.keys(obj).length === 0;
}

function unpackError({ code, message, data }) {
  const err = new Error(`${message} - ${data}`);
  err.code = code;
  Error.captureStackTrace(err, unpackError);
  return err;
}

function encodeFrameHybi17(payload) {
  var i;

  const dataLength = payload.length;

  let singleByteLength;
  let additionalLength;
  if (dataLength > kMaxTwoBytePayloadLength) {
    singleByteLength = kEightBytePayloadLengthField;
    additionalLength = Buffer.alloc(8);
    let remaining = dataLength;
    for (i = 0; i < 8; ++i) {
      additionalLength[7 - i] = remaining & 0xFF;
      remaining >>= 8;
    }
  } else if (dataLength > kMaxSingleBytePayloadLength) {
    singleByteLength = kTwoBytePayloadLengthField;
    additionalLength = Buffer.alloc(2);
    additionalLength[0] = (dataLength & 0xFF00) >> 8;
    additionalLength[1] = dataLength & 0xFF;
  } else {
    additionalLength = Buffer.alloc(0);
    singleByteLength = dataLength;
  }

  const header = Buffer.from([
    kFinalBit | kOpCodeText,
    kMaskBit | singleByteLength,
  ]);

  const mask = Buffer.alloc(4);
  const masked = Buffer.alloc(dataLength);
  for (i = 0; i < dataLength; ++i) {
    masked[i] = payload[i] ^ mask[i % kMaskingKeyWidthInBytes];
  }

  return Buffer.concat([header, additionalLength, mask, masked]);
}

function decodeFrameHybi17(data) {
  const dataAvailable = data.length;
  const notComplete = { closed: false, payload: null, rest: data };
  let payloadOffset = 2;
  if ((dataAvailable - payloadOffset) < 0) return notComplete;

  const firstByte = data[0];
  const secondByte = data[1];

  const final = (firstByte & kFinalBit) !== 0;
  const reserved1 = (firstByte & kReserved1Bit) !== 0;
  const reserved2 = (firstByte & kReserved2Bit) !== 0;
  const reserved3 = (firstByte & kReserved3Bit) !== 0;
  const opCode = firstByte & kOpCodeMask;
  const masked = (secondByte & kMaskBit) !== 0;
  const compressed = reserved1;
  if (compressed) {
    throw new Error('Compressed frames not supported');
  }
  if (!final || reserved2 || reserved3) {
    throw new Error('Only compression extension is supported');
  }

  if (masked) {
    throw new Error('Masked server frame - not supported');
  }

  let closed = false;
  switch (opCode) {
    case kOpCodeClose:
      closed = true;
      break;
    case kOpCodeText:
      break;
    default:
      throw new Error(`Unsupported op code ${opCode}`);
  }

  let payloadLength = secondByte & kPayloadLengthMask;
  switch (payloadLength) {
    case kTwoBytePayloadLengthField:
      payloadOffset += 2;
      payloadLength = (data[2] << 8) + data[3];
      break;

    case kEightBytePayloadLengthField:
      payloadOffset += 8;
      payloadLength = 0;
      for (var i = 0; i < 8; ++i) {
        payloadLength <<= 8;
        payloadLength |= data[2 + i];
      }
      break;

    default:
      // Nothing. We already have the right size.
  }
  if ((dataAvailable - payloadOffset - payloadLength) < 0) return notComplete;

  const payloadEnd = payloadOffset + payloadLength;
  return {
    payload: data.slice(payloadOffset, payloadEnd),
    rest: data.slice(payloadEnd),
    closed,
  };
}

class Client extends EventEmitter {
  constructor() {
    super();
    this.handleChunk = this._handleChunk.bind(this);

    this._port = undefined;
    this._host = undefined;

    this.reset();
  }

  _handleChunk(chunk) {
    this._unprocessed = Buffer.concat([this._unprocessed, chunk]);

    while (this._unprocessed.length > 2) {
      const {
        closed,
        payload: payloadBuffer,
        rest
      } = decodeFrameHybi17(this._unprocessed);
      this._unprocessed = rest;

      if (closed) {
        this.reset();
        return;
      }
      if (payloadBuffer === null) break;

      const payloadStr = payloadBuffer.toString();
      debuglog('< %s', payloadStr);
      const lastChar = payloadStr[payloadStr.length - 1];
      if (payloadStr[0] !== '{' || lastChar !== '}') {
        throw new Error(`Payload does not look like JSON: ${payloadStr}`);
      }
      let payload;
      try {
        payload = JSON.parse(payloadStr);
      } catch (parseError) {
        parseError.string = payloadStr;
        throw parseError;
      }

      const { id, method, params, result, error } = payload;
      if (id) {
        const handler = this._pending[id];
        if (handler) {
          delete this._pending[id];
          handler(error, result);
        }
      } else if (method) {
        this.emit('debugEvent', method, params);
        this.emit(method, params);
      } else {
        throw new Error(`Unsupported response: ${payloadStr}`);
      }
    }
  }

  reset() {
    if (this._http) {
      this._http.destroy();
    }
    this._http = null;
    this._lastId = 0;
    this._socket = null;
    this._pending = {};
    this._unprocessed = Buffer.alloc(0);
  }

  callMethod(method, params) {
    return new Promise((resolve, reject) => {
      if (!this._socket) {
        reject(new Error('Use `run` to start the app again.'));
        return;
      }
      const data = { id: ++this._lastId, method, params };
      this._pending[data.id] = (error, result) => {
        if (error) reject(unpackError(error));
        else resolve(isEmpty(result) ? undefined : result);
      };
      const json = JSON.stringify(data);
      debuglog('> %s', json);
      this._socket.write(encodeFrameHybi17(Buffer.from(json)));
    });
  }

  _fetchJSON(urlPath) {
    return new Promise((resolve, reject) => {
      const httpReq = http.get({
        host: this._host,
        port: this._port,
        path: urlPath,
      });

      const chunks = [];

      function onResponse(httpRes) {
        function parseChunks() {
          const resBody = Buffer.concat(chunks).toString();
          if (httpRes.statusCode !== 200) {
            reject(new Error(`Unexpected ${httpRes.statusCode}: ${resBody}`));
            return;
          }
          try {
            resolve(JSON.parse(resBody));
          } catch (parseError) {
            reject(new Error(`Response didn't contain JSON: ${resBody}`));
            return;
          }
        }

        httpRes.on('error', reject);
        httpRes.on('data', (chunk) => chunks.push(chunk));
        httpRes.on('end', parseChunks);
      }

      httpReq.on('error', reject);
      httpReq.on('response', onResponse);
    });
  }

  connect(port, host) {
    this._port = port;
    this._host = host;
    return this._discoverWebsocketPath()
      .then((urlPath) => this._connectWebsocket(urlPath));
  }

  _discoverWebsocketPath() {
    return this._fetchJSON('/json')
      .then(([{ webSocketDebuggerUrl }]) =>
        URL.parse(webSocketDebuggerUrl).path);
  }

  _connectWebsocket(urlPath) {
    this.reset();

    const key1 = crypto.randomBytes(16).toString('base64');
    debuglog('request websocket', key1);

    const httpReq = this._http = http.request({
      host: this._host,
      port: this._port,
      path: urlPath,
      headers: {
        Connection: 'Upgrade',
        Upgrade: 'websocket',
        'Sec-WebSocket-Key': key1,
        'Sec-WebSocket-Version': '13',
      },
    });
    httpReq.on('error', (e) => {
      this.emit('error', e);
    });
    httpReq.on('response', (httpRes) => {
      if (httpRes.statusCode >= 400) {
        process.stderr.write(`Unexpected HTTP code: ${httpRes.statusCode}\n`);
        httpRes.pipe(process.stderr);
      } else {
        httpRes.pipe(process.stderr);
      }
    });

    const handshakeListener = (res, socket) => {
      // TODO: we *could* validate res.headers[sec-websocket-accept]
      debuglog('websocket upgrade');

      this._socket = socket;
      socket.on('data', this.handleChunk);
      socket.on('close', () => {
        this.emit('close');
      });

      this.emit('ready');
    };

    return new Promise((resolve, reject) => {
      this.once('error', reject);
      this.once('ready', resolve);

      httpReq.on('upgrade', handshakeListener);
      httpReq.end();
    });
  }
}

module.exports = Client;
