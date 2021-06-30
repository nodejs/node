'use strict';

const {
  ArrayPrototypePush,
  ErrorCaptureStackTrace,
  FunctionPrototypeBind,
  JSONParse,
  JSONStringify,
  ObjectKeys,
  Promise,
} = primordials;

const Buffer = require('buffer').Buffer;
const { ERR_DEBUGGER_ERROR } = require('internal/errors').codes;
const { EventEmitter } = require('events');
const http = require('http');
const URL = require('url');

const debuglog = require('internal/util/debuglog').debuglog('inspect');

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

function unpackError({ code, message, data }) {
  const err = new ERR_DEBUGGER_ERROR(`${message} - ${data}`);
  err.code = code;
  ErrorCaptureStackTrace(err, unpackError);
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
    throw new ERR_DEBUGGER_ERROR('Compressed frames not supported');
  }
  if (!final || reserved2 || reserved3) {
    throw new ERR_DEBUGGER_ERROR('Only compression extension is supported');
  }

  if (masked) {
    throw new ERR_DEBUGGER_ERROR('Masked server frame - not supported');
  }

  let closed = false;
  switch (opCode) {
    case kOpCodeClose:
      closed = true;
      break;
    case kOpCodeText:
      break;
    default:
      throw new ERR_DEBUGGER_ERROR(`Unsupported op code ${opCode}`);
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
    this.handleChunk = FunctionPrototypeBind(this._handleChunk, this);

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
      if (payloadBuffer === null || payloadBuffer.length === 0) break;

      const payloadStr = payloadBuffer.toString();
      debuglog('< %s', payloadStr);
      const lastChar = payloadStr[payloadStr.length - 1];
      if (payloadStr[0] !== '{' || lastChar !== '}') {
        throw new ERR_DEBUGGER_ERROR(`Payload does not look like JSON: ${payloadStr}`);
      }
      let payload;
      try {
        payload = JSONParse(payloadStr);
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
        throw new ERR_DEBUGGER_ERROR(`Unsupported response: ${payloadStr}`);
      }
    }
  }

  reset() {
    if (this._http) {
      this._http.destroy();
    }
    if (this._socket) {
      this._socket.destroy();
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
        reject(new ERR_DEBUGGER_ERROR('Use `run` to start the app again.'));
        return;
      }
      const data = { id: ++this._lastId, method, params };
      this._pending[data.id] = (error, result) => {
        if (error) reject(unpackError(error));
        else resolve(ObjectKeys(result).length ? result : undefined);
      };
      const json = JSONStringify(data);
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
            reject(new ERR_DEBUGGER_ERROR(`Unexpected ${httpRes.statusCode}: ${resBody}`));
            return;
          }
          try {
            resolve(JSONParse(resBody));
          } catch {
            reject(new ERR_DEBUGGER_ERROR(`Response didn't contain JSON: ${resBody}`));

          }
        }

        httpRes.on('error', reject);
        httpRes.on('data', (chunk) => ArrayPrototypePush(chunks, chunk));
        httpRes.on('end', parseChunks);
      }

      httpReq.on('error', reject);
      httpReq.on('response', onResponse);
    });
  }

  async connect(port, host) {
    this._port = port;
    this._host = host;
    const urlPath = await this._discoverWebsocketPath();
    return this._connectWebsocket(urlPath);
  }

  async _discoverWebsocketPath() {
    const { 0: { webSocketDebuggerUrl } } = await this._fetchJSON('/json');
    return URL.parse(webSocketDebuggerUrl).path;
  }

  _connectWebsocket(urlPath) {
    this.reset();

    const key1 = require('crypto').randomBytes(16).toString('base64');
    debuglog('request websocket', key1);

    const httpReq = this._http = http.request({
      host: this._host,
      port: this._port,
      path: urlPath,
      headers: {
        'Connection': 'Upgrade',
        'Upgrade': 'websocket',
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
