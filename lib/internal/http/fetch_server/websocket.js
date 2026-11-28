'use strict';

// WebSocket upgrades of the Fetch Server API (Section 11.4, Section 11.5,
// Section 13), with the RFC 6455 framing and the RFC 7692 permessage-deflate
// extension.

const {
  ArrayPrototypePush,
  ArrayPrototypeShift,
  MathFloor,
  ObjectDefineProperties,
  Promise,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  RegExpPrototypeExec,
  SafeSet,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeToLowerCase,
  StringPrototypeToWellFormed,
  StringPrototypeTrim,
  Symbol,
  SymbolAsyncDispose,
  SymbolToStringTag,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeSet,
  TypedArrayPrototypeSlice,
  TypedArrayPrototypeSubarray,
  Uint8Array,
} = primordials;

const { Buffer } = require('buffer');
const { URL } = require('internal/url');
const { setTimeout, clearTimeout, setImmediate } = require('timers');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  kEnumerableProperty,
  lazyDOMException,
} = require('internal/util');

const {
  isAnyArrayBuffer,
  isArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');

const {
  converters,
  convertToInt,
  createSequenceConverter,
} = require('internal/webidl');

const {
  Event,
  EventTarget,
  defineEventHandler,
} = require('internal/event_target');

const {
  createProtocolError,
  getErrorCode,
  invalidState,
  kConnectError,
  kConnectionReset,
  kH2Codes,
  kRequestCancelled,
  kTimeout,
  kTunnelSendable,
  notSupported,
  protocolErrorFromH2,
} = require('internal/http/fetch_server/errors');

const {
  listGet,
  listGetAll,
} = require('internal/http/fetch_server/headers');

const {
  markHandled,
} = require('internal/http/fetch_server/body');

const kConstruct = Symbol('kConstruct');
const kWebSocketGuid = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

// Implementation-defined limits (Section 13.2).
const kMaxMessageSize = 64 * 1024 * 1024;
const kReadHighWaterMark = 1024 * 1024;
const kCompressionThreshold = 1024;
const kDefaultCloseHandshakeTimeout = 30_000;

const kOpContinuation = 0x0;
const kOpText = 0x1;
const kOpBinary = 0x2;
const kOpClose = 0x8;
const kOpPing = 0x9;
const kOpPong = 0xa;

const kDeflateTrailer = new Uint8Array([0x00, 0x00, 0xff, 0xff]);
const kEmpty = new Uint8Array(0);

let lazyTextDecoder;
function decodeUtf8(bytes) {
  if (lazyTextDecoder === undefined) {
    const { TextDecoder } = require('internal/encoding');
    lazyTextDecoder = new TextDecoder('utf-8', { fatal: true, ignoreBOM: true });
  }
  return lazyTextDecoder.decode(bytes);
}

function encodeUtf8(string) {
  const buffer = Buffer.from(string, 'utf8');
  const out = new Uint8Array(buffer.length);
  TypedArrayPrototypeSet(out, buffer, 0);
  return out;
}

function concatBytes(list, length) {
  if (list.length === 1 && TypedArrayPrototypeGetByteLength(list[0]) === length) {
    return list[0];
  }
  const out = new Uint8Array(length);
  let offset = 0;
  for (let i = 0; i < list.length; i++) {
    TypedArrayPrototypeSet(out, list[i], offset);
    offset += TypedArrayPrototypeGetByteLength(list[i]);
  }
  return out;
}

// =============================================================================
// Options (WebSocketUpgradeInit) and close codes (Section 13.4)
// =============================================================================

const convertStringSequence = createSequenceConverter(converters.DOMString);

function convertUpgradeOptions(options) {
  const result = { __proto__: null, protocol: undefined,
                   compression: undefined, pingInterval: undefined };
  if (options === undefined || options === null) return result;
  if (typeof options !== 'object' && typeof options !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
  }
  // Dictionary members are converted in lexicographical order.
  const compression = options.compression;
  if (compression !== undefined) result.compression = !!compression;
  const pingInterval = options.pingInterval;
  if (pingInterval !== undefined) {
    result.pingInterval = convertToInt(pingInterval, 64, 'unsigned', {
      __proto__: null, enforceRange: true, context: 'options.pingInterval',
    });
  }
  const protocol = options.protocol;
  if (protocol !== undefined) {
    result.protocol = convertStringSequence(protocol, {
      __proto__: null, context: 'options.protocol',
    });
  }
  return result;
}

function convertCloseInfo(info) {
  const result = { __proto__: null, closeCode: null, reason: '' };
  if (info === undefined || info === null) return result;
  if (typeof info !== 'object' && typeof info !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('closeInfo', 'Object', info);
  }
  const closeCode = info.closeCode;
  if (closeCode !== undefined) {
    result.closeCode = converters['unsigned short'](closeCode, {
      __proto__: null, enforceRange: true, context: 'closeInfo.closeCode',
    });
  }
  const reason = info.reason;
  if (reason !== undefined) {
    result.reason = converters.USVString(reason, {
      __proto__: null, context: 'closeInfo.reason',
    });
  }
  return result;
}

function isSendableCloseCode(code) {
  return (code >= 1000 && code <= 1003) || (code >= 1007 && code <= 1009) ||
         (code >= 1011 && code <= 1014) || (code >= 3000 && code <= 4999);
}

// The codes a peer may send in a Close frame (RFC 6455 Section 7.4).
function isReceivableCloseCode(code) {
  return isSendableCloseCode(code) || code === 1010;
}

/**
 * Validates a server close code and reason (Section 13.4).
 * @param {number|null} code
 * @param {string} reason
 */
function validateServerClose(code, reason) {
  if (code !== null && !isSendableCloseCode(code)) {
    throw lazyDOMException(`The close code ${code} cannot be sent by a server`,
                           'InvalidAccessError');
  }
  if (Buffer.byteLength(reason, 'utf8') > 123) {
    throw lazyDOMException('The close reason is longer than 123 bytes',
                           'SyntaxError');
  }
}

// =============================================================================
// Opening handshake (Section 11.4)
// =============================================================================

const kTokenRegExp = /^[!#$%&'*+\-.^_`|~0-9A-Za-z]+$/;
const kKeyRegExp = /^[A-Za-z0-9+/]{21}[AQgw]==$/;

function splitList(value) {
  const result = [];
  const parts = StringPrototypeSplit(value, ',');
  for (let i = 0; i < parts.length; i++) {
    const part = StringPrototypeTrim(parts[i]);
    if (part !== '') ArrayPrototypePush(result, part);
  }
  return result;
}

/**
 * Checks a WebSocket handshake request (Section 11.4).
 * @param {import('./exchange').Exchange} exchange
 * @returns {boolean}
 */
function checkHandshake(exchange) {
  const list = exchange.backend.headerList;
  const version = listGet(list, 'sec-websocket-version');
  if (version === null || StringPrototypeTrim(version) !== '13') return false;
  if (exchange.kind !== 'upgrade') return true;
  if (exchange.backend.method !== 'GET') return false;
  const upgrade = listGet(list, 'upgrade');
  if (upgrade === null) return false;
  const offered = splitList(upgrade);
  let found = false;
  for (let i = 0; i < offered.length; i++) {
    if (StringPrototypeToLowerCase(offered[i]) === 'websocket') found = true;
  }
  if (!found) return false;
  const key = listGet(list, 'sec-websocket-key');
  return key !== null &&
    RegExpPrototypeExec(kKeyRegExp, StringPrototypeTrim(key)) !== null;
}

function computeAccept(key) {
  let createHash;
  try {
    ({ createHash } = require('crypto'));
  } catch {
    throw notSupported('WebSocket upgrades over HTTP/1.1 require crypto');
  }
  return createHash('sha1').update(`${key}${kWebSocketGuid}`)
    .digest('base64');
}

// Splits on sep outside of quoted strings.
function splitOutsideQuotes(value, sep) {
  const result = [];
  let start = 0;
  let quoted = false;
  for (let i = 0; i < value.length; i++) {
    const c = value[i];
    if (quoted) {
      if (c === '\\') i++;
      else if (c === '"') quoted = false;
    } else if (c === '"') {
      quoted = true;
    } else if (c === sep) {
      ArrayPrototypePush(result, StringPrototypeSlice(value, start, i));
      start = i + 1;
    }
  }
  if (quoted) return null;
  ArrayPrototypePush(result, StringPrototypeSlice(value, start));
  return result;
}

function unquote(value) {
  if (value[0] !== '"') return value;
  let result = '';
  for (let i = 1; i < value.length - 1; i++) {
    if (value[i] === '\\') i++;
    result += value[i];
  }
  return result;
}

/**
 * Parses Sec-WebSocket-Extensions offers (RFC 6455 Section 9.1).
 * @param {string} value
 * @returns {Array<{ name: string, params: Array<[string, string|true]> }>}
 */
function parseExtensions(value) {
  const offers = [];
  const extensions = splitOutsideQuotes(value, ',');
  if (extensions === null) return offers;
  for (let i = 0; i < extensions.length; i++) {
    const parts = splitOutsideQuotes(extensions[i], ';');
    const name = StringPrototypeToLowerCase(StringPrototypeTrim(parts[0]));
    if (name === '') continue;
    if (RegExpPrototypeExec(kTokenRegExp, name) === null) continue;
    const params = [];
    let valid = true;
    for (let j = 1; j < parts.length; j++) {
      const param = StringPrototypeTrim(parts[j]);
      const eq = StringPrototypeIndexOf(param, '=');
      const key = StringPrototypeToLowerCase(StringPrototypeTrim(
        eq === -1 ? param : StringPrototypeSlice(param, 0, eq)));
      if (RegExpPrototypeExec(kTokenRegExp, key) === null) {
        valid = false;
        break;
      }
      const paramValue = eq === -1 ? true :
        unquote(StringPrototypeTrim(StringPrototypeSlice(param, eq + 1)));
      ArrayPrototypePush(params, [key, paramValue]);
    }
    if (valid) ArrayPrototypePush(offers, { name, params });
  }
  return offers;
}

const kWindowBitsRegExp = /^(?:[89]|1[0-5])$/;

/**
 * Selects the first acceptable permessage-deflate offer (RFC 7692 Section 7).
 * @param {string|null} header
 * @returns {object|null}
 */
function negotiateDeflate(header) {
  if (header === null) return null;
  const offers = parseExtensions(header);
  for (let i = 0; i < offers.length; i++) {
    const offer = offers[i];
    if (offer.name !== 'permessage-deflate') continue;
    const seen = new SafeSet();
    let ok = true;
    let serverNoContextTakeover = false;
    let clientNoContextTakeover = false;
    let serverMaxWindowBits;
    for (let j = 0; j < offer.params.length && ok; j++) {
      const { 0: key, 1: value } = offer.params[j];
      if (seen.has(key)) {
        ok = false;
        break;
      }
      seen.add(key);
      switch (key) {
        case 'server_no_context_takeover':
          if (value !== true) ok = false;
          serverNoContextTakeover = true;
          break;
        case 'client_no_context_takeover':
          if (value !== true) ok = false;
          clientNoContextTakeover = true;
          break;
        case 'server_max_window_bits':
          // The zlib library cannot produce a raw deflate stream with an 8-bit
          // window.
          if (value === true ||
              RegExpPrototypeExec(kWindowBitsRegExp, value) === null ||
              value === '8') {
            ok = false;
          } else {
            serverMaxWindowBits = +value;
          }
          break;
        case 'client_max_window_bits':
          if (value !== true &&
              RegExpPrototypeExec(kWindowBitsRegExp, value) === null) {
            ok = false;
          }
          break;
        default:
          ok = false;
      }
    }
    if (!ok) continue;
    let response = 'permessage-deflate';
    if (serverNoContextTakeover) response += '; server_no_context_takeover';
    if (clientNoContextTakeover) response += '; client_no_context_takeover';
    if (serverMaxWindowBits !== undefined) {
      response += `; server_max_window_bits=${serverMaxWindowBits}`;
    }
    return {
      __proto__: null,
      response,
      serverNoContextTakeover,
      windowBits: serverMaxWindowBits ?? 15,
    };
  }
  return null;
}

// =============================================================================
// Framing (RFC 6455 Section 5)
// =============================================================================

function encodeFrameHeader(opcode, length, rsv1) {
  let header;
  if (length <= 125) {
    header = new Uint8Array(2);
    header[1] = length;
  } else if (length <= 0xffff) {
    header = new Uint8Array(4);
    header[1] = 126;
    header[2] = length >>> 8;
    header[3] = length & 0xff;
  } else {
    header = new Uint8Array(10);
    header[1] = 127;
    const high = MathFloor(length / 0x100000000);
    const low = length >>> 0;
    header[2] = high >>> 24;
    header[3] = (high >>> 16) & 0xff;
    header[4] = (high >>> 8) & 0xff;
    header[5] = high & 0xff;
    header[6] = low >>> 24;
    header[7] = (low >>> 16) & 0xff;
    header[8] = (low >>> 8) & 0xff;
    header[9] = low & 0xff;
  }
  header[0] = 0x80 | (rsv1 ? 0x40 : 0) | opcode;
  return header;
}

function encodeClosePayload(code, reason) {
  if (code === null) return kEmpty;
  const reasonBytes = encodeUtf8(reason);
  const payload = new Uint8Array(2 + reasonBytes.length);
  payload[0] = code >>> 8;
  payload[1] = code & 0xff;
  TypedArrayPrototypeSet(payload, reasonBytes, 2);
  return payload;
}

class WebSocketFailure {
  constructor(code, message) {
    this.code = code;
    this.message = message;
  }
}

// =============================================================================
// Connection internals
// =============================================================================

class WebSocketConnection {
  exchange;
  stream;
  http2;
  // Push mode: messages are passed to onMessage as they arrive. Pull mode:
  // they are queued for nextMessage(), with transport backpressure.
  pull;
  onMessage = null;
  onClosing = null;
  onClean = null;
  onAbnormal = null;

  closeSent = false;
  closeReceived = false;
  terminated = false;
  clean = false;
  error = undefined;
  peerCode = 1005;
  peerReason = '';
  bufferedAmount = 0;
  closed = PromiseWithResolvers();

  #chunks = [];
  #buffered = 0;
  #frame = null;
  #message = null;
  #suspended = false;
  #queue = [];
  #queuedBytes = 0;
  #paused = false;
  #waiters = [];
  #drains = [];
  #sendChain = PromiseResolve();
  #deflate;
  #deflater = null;
  #inflater = null;
  #pingInterval;
  #pingTimer = null;
  #idleTimeout;
  #idleTimer = null;
  #closeTimeout;
  #closeTimer = null;

  constructor(exchange, transport, { pull, deflate, pingInterval }) {
    this.exchange = exchange;
    this.stream = transport.stream;
    this.http2 = transport.http2;
    this.pull = pull;
    this.#deflate = deflate;
    const timeouts = exchange.listener?.options?.timeouts ?? {};
    this.#pingInterval = pingInterval ?? 0;
    this.#idleTimeout = timeouts.tunnelIdle ?? 0;
    this.#closeTimeout = timeouts.closeHandshake ??
      kDefaultCloseHandshakeTimeout;
    markHandled(this.closed.promise);

    const stream = this.stream;
    stream.on('error', () => {});
    stream.on('drain', () => this.#wakeDrains());
    stream.on('data', (chunk) => this.#onData(chunk));
    stream.on('end', () => this.#onEnd());
    stream.on('close', () => this.#onClose());
    this.#armPing();
    this.#armIdle();
  }

  // Timers.

  #armPing() {
    if (this.#pingInterval === 0 || this.closeSent || this.terminated) return;
    if (this.#pingTimer === null) {
      this.#pingTimer = setTimeout(() => {
        this.#pingTimer = null;
        if (this.closeSent || this.terminated) return;
        this.#writeFrame(kOpPing, kEmpty, false);
        this.#armPing();
      }, this.#pingInterval);
      this.#pingTimer.unref();
    } else {
      this.#pingTimer.refresh();
    }
  }

  #armIdle() {
    if (this.#idleTimeout === 0 || this.terminated) return;
    if (this.#idleTimer === null) {
      this.#idleTimer = setTimeout(() => {
        this.#idleTimer = null;
        this.#timeout('The WebSocket connection was idle for too long');
      }, this.#idleTimeout);
      this.#idleTimer.unref();
    } else {
      this.#idleTimer.refresh();
    }
  }

  #clearTimers() {
    if (this.#pingTimer !== null) clearTimeout(this.#pingTimer);
    if (this.#idleTimer !== null) clearTimeout(this.#idleTimer);
    if (this.#closeTimer !== null) clearTimeout(this.#closeTimer);
    this.#pingTimer = this.#idleTimer = this.#closeTimer = null;
  }

  #timeout(message) {
    if (this.terminated) return;
    this.#resetTransport(kRequestCancelled);
    this.#terminate(createProtocolError(kTimeout, message), true);
  }

  // Inbound.

  #onData(chunk) {
    if (this.terminated || this.closeReceived) return;
    this.#armPing();
    this.#armIdle();
    ArrayPrototypePush(this.#chunks, chunk);
    this.#buffered += chunk.length;
    this.#process();
  }

  #onEnd() {
    if (this.terminated || this.closeReceived) return;
    // The peer closed its direction without a closing handshake.
    this.#closeTransport();
    this.#terminate(createProtocolError(
      kConnectError,
      'The connection was closed before the closing handshake'), true);
  }

  #onClose() {
    if (this.terminated) return;
    if (this.closeSent && this.closeReceived) {
      this.#finishClean();
      return;
    }
    let reason;
    const rstCode = this.http2 ? this.stream.rstCode : undefined;
    if (rstCode !== undefined && rstCode !== 0) {
      reason = protocolErrorFromH2(rstCode, 'h2');
    } else {
      reason = createProtocolError(kConnectionReset, 'The connection was lost');
    }
    this.#terminate(reason, true);
  }

  #consume(length) {
    const out = new Uint8Array(length);
    let offset = 0;
    while (offset < length) {
      const chunk = this.#chunks[0];
      const take = length - offset;
      if (chunk.length <= take) {
        TypedArrayPrototypeSet(out, chunk, offset);
        offset += chunk.length;
        ArrayPrototypeShift(this.#chunks);
      } else {
        TypedArrayPrototypeSet(out, TypedArrayPrototypeSubarray(chunk, 0, take),
                               offset);
        this.#chunks[0] = TypedArrayPrototypeSubarray(chunk, take);
        offset = length;
      }
    }
    this.#buffered -= length;
    return out;
  }

  #peek(length) {
    const first = this.#chunks[0];
    if (first.length >= length) return first;
    return concatBytes(this.#chunks, this.#buffered);
  }

  #parseHeader() {
    if (this.#buffered < 2) return null;
    const start = this.#peek(2);
    const b0 = start[0];
    const b1 = start[1];
    const lengthCode = b1 & 0x7f;
    const masked = (b1 & 0x80) !== 0;
    const needed = 2 + (lengthCode === 126 ? 2 : lengthCode === 127 ? 8 : 0) +
      (masked ? 4 : 0);
    if (this.#buffered < needed) return null;
    const header = this.#consume(needed);
    const fin = (b0 & 0x80) !== 0;
    const rsv1 = (b0 & 0x40) !== 0;
    const opcode = b0 & 0x0f;
    let length = lengthCode;
    let offset = 2;
    if (lengthCode === 126) {
      length = (header[2] << 8) | header[3];
      offset = 4;
    } else if (lengthCode === 127) {
      const high = ((header[2] << 24) | (header[3] << 16) |
                    (header[4] << 8) | header[5]) >>> 0;
      const low = ((header[6] << 24) | (header[7] << 16) |
                   (header[8] << 8) | header[9]) >>> 0;
      if (high > 0x1fffff) {
        throw new WebSocketFailure(1009, 'The frame is too large');
      }
      length = high * 0x100000000 + low;
      offset = 10;
    }
    if ((b0 & 0x30) !== 0) {
      throw new WebSocketFailure(1002, 'RSV2 and RSV3 must be clear');
    }
    if (!masked) {
      throw new WebSocketFailure(1002, 'Client frames must be masked');
    }
    if (opcode >= 0x8) {
      if (opcode !== kOpClose && opcode !== kOpPing && opcode !== kOpPong) {
        throw new WebSocketFailure(1002, `Unknown opcode ${opcode}`);
      }
      if (!fin || rsv1 || length > 125) {
        throw new WebSocketFailure(1002, 'Invalid control frame');
      }
    } else if (opcode === kOpContinuation) {
      if (this.#message === null) {
        throw new WebSocketFailure(1002, 'Unexpected continuation frame');
      }
      if (rsv1) throw new WebSocketFailure(1002, 'RSV1 must be clear');
    } else if (opcode === kOpText || opcode === kOpBinary) {
      if (this.#message !== null) {
        throw new WebSocketFailure(1002, 'Expected a continuation frame');
      }
      if (rsv1 && this.#deflate === null) {
        throw new WebSocketFailure(1002, 'RSV1 must be clear');
      }
    } else {
      throw new WebSocketFailure(1002, `Unknown opcode ${opcode}`);
    }
    if (opcode < 0x8) {
      const total = (this.#message?.length ?? 0) + length;
      if (total > kMaxMessageSize) {
        throw new WebSocketFailure(1009, 'The message is too large');
      }
    }
    return {
      __proto__: null,
      fin,
      rsv1,
      opcode,
      length,
      mask: TypedArrayPrototypeSubarray(header, offset, offset + 4),
    };
  }

  #process() {
    try {
      while (!this.#suspended && !this.terminated && !this.closeReceived) {
        if (this.#frame === null) {
          this.#frame = this.#parseHeader();
          if (this.#frame === null) break;
        }
        const frame = this.#frame;
        if (this.#buffered < frame.length) break;
        this.#frame = null;
        const payload = frame.length === 0 ? kEmpty : this.#consume(frame.length);
        const mask = frame.mask;
        for (let i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];
        this.#handleFrame(frame, payload);
      }
    } catch (err) {
      if (err instanceof WebSocketFailure) {
        this.fail(err.code, err.message);
        return;
      }
      throw err;
    }
  }

  #handleFrame(frame, payload) {
    switch (frame.opcode) {
      case kOpPing:
        if (!this.closeSent) this.#writeFrame(kOpPong, payload, false);
        return;
      case kOpPong:
        return;
      case kOpClose:
        this.#handlePeerClose(payload);
        return;
      default:
        break;
    }
    if (frame.opcode !== kOpContinuation) {
      this.#message = {
        __proto__: null,
        text: frame.opcode === kOpText,
        compressed: frame.rsv1,
        fragments: [],
        length: 0,
      };
    }
    const message = this.#message;
    ArrayPrototypePush(message.fragments, payload);
    message.length += payload.length;
    if (!frame.fin) return;
    this.#message = null;
    const data = concatBytes(message.fragments, message.length);
    if (!message.compressed) {
      this.#finishMessage(message.text, data);
      return;
    }
    this.#suspended = true;
    PromisePrototypeThen(this.#inflate(data), (inflated) => {
      this.#suspended = false;
      if (this.terminated) return;
      try {
        this.#finishMessage(message.text, inflated);
      } catch (err) {
        this.fail(err.code, err.message);
        return;
      }
      this.#process();
    }, (err) => {
      this.#suspended = false;
      if (err instanceof WebSocketFailure) this.fail(err.code, err.message);
      else this.fail(1007, 'Invalid compressed data');
    });
  }

  #finishMessage(text, bytes) {
    let data = bytes;
    if (text) {
      try {
        data = decodeUtf8(bytes);
      } catch {
        throw new WebSocketFailure(1007, 'Invalid UTF-8 in a text message');
      }
    }
    if (!this.pull) {
      this.onMessage(data);
      return;
    }
    ArrayPrototypePush(this.#queue, data);
    this.#queuedBytes += bytes.length;
    if (this.#queuedBytes > kReadHighWaterMark && !this.#paused) {
      this.#paused = true;
      this.stream.pause();
    }
    this.#wakeWaiters();
  }

  #handlePeerClose(payload) {
    if (payload.length === 1) {
      throw new WebSocketFailure(1002, 'Invalid Close frame');
    }
    let code = 1005;
    let reason = '';
    if (payload.length >= 2) {
      code = (payload[0] << 8) | payload[1];
      if (!isReceivableCloseCode(code)) {
        throw new WebSocketFailure(1002, `Invalid close code ${code}`);
      }
      try {
        reason = decodeUtf8(TypedArrayPrototypeSubarray(payload, 2));
      } catch {
        throw new WebSocketFailure(1007, 'Invalid UTF-8 in a close reason');
      }
    }
    this.closeReceived = true;
    this.peerCode = code;
    this.peerReason = reason;
    this.#message = null;
    this.#chunks = [];
    this.#buffered = 0;
    if (this.#paused) {
      this.#paused = false;
      this.stream.resume();
    }
    this.onClosing?.();
    this.#wakeWaiters();
    if (!this.closeSent) {
      // Echo the status code (RFC 6455 Section 5.5.1).
      this.#sendClose(code === 1005 ? null : code, '');
    } else {
      this.#finishHandshake();
    }
  }

  /**
   * Returns the next message for pull mode: a string, a Uint8Array, or null
   * once the peer has started the closing handshake and every message has
   * been read.
   * @returns {Promise<string|Uint8Array|null>}
   */
  async nextMessage() {
    for (;;) {
      if (this.terminated && !this.clean) throw this.error;
      if (this.#queue.length > 0) {
        const message = ArrayPrototypeShift(this.#queue);
        this.#queuedBytes -= typeof message === 'string' ?
          Buffer.byteLength(message) : message.length;
        if (this.#paused && this.#queuedBytes <= kReadHighWaterMark / 2) {
          this.#paused = false;
          this.stream.resume();
        }
        return message;
      }
      if (this.closeReceived || this.terminated) return null;
      const { promise, resolve } = PromiseWithResolvers();
      ArrayPrototypePush(this.#waiters, resolve);
      await promise;
    }
  }

  #wakeWaiters() {
    const waiters = this.#waiters;
    this.#waiters = [];
    for (let i = 0; i < waiters.length; i++) waiters[i]();
  }

  #inflate(data) {
    this.#inflater ??= require('zlib').createInflateRaw({
      maxOutputLength: kMaxMessageSize,
    });
    const inflater = this.#inflater;
    return new Promise((resolve, reject) => {
      const chunks = [];
      let length = 0;
      const onData = (chunk) => {
        length += chunk.length;
        if (length > kMaxMessageSize) {
          cleanup();
          reject(new WebSocketFailure(1009, 'The message is too large'));
          return;
        }
        ArrayPrototypePush(chunks, chunk);
      };
      const onError = (err) => {
        cleanup();
        reject(err);
      };
      const cleanup = () => {
        inflater.removeListener('data', onData);
        inflater.removeListener('error', onError);
      };
      inflater.on('data', onData);
      inflater.on('error', onError);
      inflater.write(data);
      inflater.write(kDeflateTrailer);
      inflater.flush(() => {
        cleanup();
        if (length <= kMaxMessageSize) resolve(concatBytes(chunks, length));
      });
    });
  }

  #deflateMessage(data) {
    const options = this.#deflate;
    this.#deflater ??= require('zlib').createDeflateRaw({
      windowBits: options.windowBits,
    });
    const deflater = this.#deflater;
    const { Z_SYNC_FLUSH } = require('zlib').constants;
    return new Promise((resolve, reject) => {
      const chunks = [];
      let length = 0;
      const onData = (chunk) => {
        ArrayPrototypePush(chunks, chunk);
        length += chunk.length;
      };
      const onError = (err) => {
        deflater.removeListener('data', onData);
        reject(err);
      };
      deflater.on('data', onData);
      deflater.once('error', onError);
      deflater.write(data);
      deflater.flush(Z_SYNC_FLUSH, () => {
        deflater.removeListener('data', onData);
        deflater.removeListener('error', onError);
        let out = concatBytes(chunks, length);
        if (length >= 4) out = TypedArrayPrototypeSubarray(out, 0, length - 4);
        if (out.length === 0) out = new Uint8Array(1);
        if (options.serverNoContextTakeover) deflater.reset();
        resolve(out);
      });
    });
  }

  // Outbound.

  #writeFrame(opcode, payload, rsv1, callback) {
    const header = encodeFrameHeader(opcode, payload.length, rsv1);
    this.#armIdle();
    if (payload.length === 0) {
      return this.stream.write(header, callback);
    }
    if (payload.length <= 16384) {
      const frame = new Uint8Array(header.length + payload.length);
      TypedArrayPrototypeSet(frame, header, 0);
      TypedArrayPrototypeSet(frame, payload, header.length);
      return this.stream.write(frame, callback);
    }
    this.stream.write(header);
    return this.stream.write(payload, callback);
  }

  #wakeDrains() {
    const drains = this.#drains;
    this.#drains = [];
    for (let i = 0; i < drains.length; i++) drains[i]();
  }

  #checkSendable() {
    if (this.terminated && !this.clean) throw this.error;
    if (this.closeSent || this.terminated) {
      throw invalidState('The WebSocket closing handshake has started');
    }
  }

  /**
   * Sends a message. The returned promise resolves once the message has been
   * handed to the transport, waiting while the transport is not accepting
   * data.
   * @param {boolean} text
   * @param {Uint8Array|Promise<Uint8Array>} bytes
   * @param {Function} [onWritten]
   * @returns {Promise<void>}
   */
  sendMessage(text, bytes, onWritten) {
    try {
      this.#checkSendable();
    } catch (err) {
      return PromiseReject(err);
    }
    const send = async () => {
      let data = await bytes;
      this.#checkSendable();
      while (this.stream.writableNeedDrain) {
        const { promise, resolve } = PromiseWithResolvers();
        ArrayPrototypePush(this.#drains, resolve);
        await promise;
        this.#checkSendable();
      }
      let rsv1 = false;
      if (this.#deflate !== null && data.length >= kCompressionThreshold) {
        data = await this.#deflateMessage(data);
        rsv1 = true;
        this.#checkSendable();
      }
      this.#writeFrame(text ? kOpText : kOpBinary, data, rsv1, onWritten);
    };
    const result = PromisePrototypeThen(this.#sendChain, send);
    this.#sendChain = PromisePrototypeThen(result, undefined, () => {});
    return result;
  }

  #sendClose(code, reason) {
    if (this.closeSent || this.terminated) return;
    this.closeSent = true;
    if (this.#pingTimer !== null) {
      clearTimeout(this.#pingTimer);
      this.#pingTimer = null;
    }
    this.#writeFrame(kOpClose, encodeClosePayload(code, reason), false);
    // Messages that are not yet sent are discarded (Section 13.3).
    this.#wakeDrains();
    if (this.closeReceived) {
      this.#finishHandshake();
    } else if (this.#closeTimeout !== 0) {
      this.#closeTimer = setTimeout(() => {
        this.#closeTimer = null;
        this.#timeout('The peer did not complete the closing handshake');
      }, this.#closeTimeout);
      this.#closeTimer.unref();
    }
  }

  /**
   * The close() steps of Section 13.3, after validation.
   * @param {number|null} code
   * @param {string} reason
   */
  close(code, reason) {
    validateServerClose(code, reason);
    if (this.closeSent || this.terminated) return;
    if (reason !== '' && code === null) code = 1000;
    this.#sendClose(code, reason);
  }

  /**
   * The close() steps of a WebSocket: messages queued by send() before the
   * call are still sent (WebSockets Standard).
   * @param {number|null} code
   * @param {string} reason
   */
  closeAfterQueued(code, reason) {
    validateServerClose(code, reason);
    if (this.closeSent || this.terminated) return;
    if (reason !== '' && code === null) code = 1000;
    PromisePrototypeThen(this.#sendChain, () => this.#sendClose(code, reason));
  }

  // Starts the closing handshake for a cancel or abort reason (Section 13.3).
  closeWithReason(reason) {
    let info;
    try {
      info = convertCloseInfo(reason);
      validateServerClose(info.closeCode, info.reason);
    } catch {
      info = { __proto__: null, closeCode: null, reason: '' };
    }
    if (this.closeSent || this.terminated) return;
    let code = info.closeCode;
    if (info.reason !== '' && code === null) code = 1000;
    this.#sendClose(code, info.reason);
  }

  windDown() {
    if (this.closeSent || this.terminated) return;
    this.#sendClose(1001, '');
  }

  #finishHandshake() {
    if (this.#closeTimer !== null) {
      clearTimeout(this.#closeTimer);
      this.#closeTimer = null;
    }
    this.#closeTransport();
  }

  // Closes the transport in an orderly way: the server closes the TCP
  // connection first (RFC 6455 Section 7.1.1); on HTTP/2 it ends the stream.
  #closeTransport() {
    const stream = this.stream;
    if (stream.destroyed) return;
    if (!this.http2) {
      stream.destroySoon();
      return;
    }
    stream.end(() => {
      if (!stream.destroyed && !stream._readableState.ended) {
        setImmediate(() => {
          if (!stream.destroyed) stream.close(kH2Codes.ERR_HTTP_REQUEST_BODY_REJECTED);
        });
      }
    });
  }

  #resetTransport(code) {
    const stream = this.stream;
    if (stream.destroyed) return;
    if (this.http2) {
      stream.close(kH2Codes[code] ?? kH2Codes[kRequestCancelled]);
    } else {
      stream.destroy();
    }
  }

  /**
   * Fails the WebSocket connection (RFC 6455 Section 7.1.7).
   * @param {number} code
   * @param {string} message
   */
  fail(code, message) {
    if (this.terminated) return;
    if (!this.closeSent) {
      this.closeSent = true;
      this.#writeFrame(kOpClose, encodeClosePayload(code, ''), false);
    }
    this.#closeTransport();
    this.#terminate(createProtocolError(kConnectError, message), true);
  }

  destroy(error) {
    if (this.terminated) return;
    const code = getErrorCode(error, kTunnelSendable) ?? kRequestCancelled;
    this.#resetTransport(code);
    this.#terminate(error ?? createProtocolError(
      kRequestCancelled, 'The WebSocket connection was destroyed'), false);
  }

  #finishClean() {
    if (this.terminated) return;
    this.terminated = true;
    this.clean = true;
    this.#clearTimers();
    this.#wakeWaiters();
    this.#wakeDrains();
    const info = { __proto__: null, closeCode: this.peerCode,
                   reason: this.peerReason };
    this.closed.resolve(info);
    this.onClean?.(info);
    this.exchange.complete();
  }

  /**
   * Terminates the connection abnormally.
   * @param {any} error
   * @param {boolean} aborted Whether the exchange is aborted, rather than
   *   terminated by the server.
   */
  #terminate(error, aborted) {
    if (this.terminated) return;
    this.terminated = true;
    this.error = error;
    this.#clearTimers();
    this.#wakeWaiters();
    this.#wakeDrains();
    this.closed.reject(error);
    this.onAbnormal?.(error);
    if (aborted) this.exchange.abort(error);
    else this.exchange.complete();
  }
}

// =============================================================================
// Establishing the connection (Section 11.4)
// =============================================================================

/**
 * Performs the WebSocket server handshake.
 * @param {import('./exchange').Exchange} exchange
 * @param {object} options Converted WebSocketUpgradeInit.
 * @param {boolean} pull
 * @returns {{ connection: WebSocketConnection, protocol: string,
 *   extensions: string }}
 */
function performHandshake(exchange, options, pull) {
  const list = exchange.backend.headerList;
  const offered = [];
  const protocolFields = listGetAll(list, 'sec-websocket-protocol');
  for (let i = 0; i < protocolFields.length; i++) {
    const items = splitList(protocolFields[i]);
    for (let j = 0; j < items.length; j++) ArrayPrototypePush(offered, items[j]);
  }
  let protocol = '';
  if (options.protocol !== undefined) {
    for (let i = 0; i < options.protocol.length && protocol === ''; i++) {
      for (let j = 0; j < offered.length; j++) {
        if (offered[j] === options.protocol[i]) {
          protocol = offered[j];
          break;
        }
      }
    }
  }
  let deflate = null;
  if (options.compression === true) {
    deflate = negotiateDeflate(listGet(list, 'sec-websocket-extensions'));
  }
  const extensions = deflate?.response ?? '';

  const fields = [];
  if (exchange.kind === 'upgrade') {
    const key = StringPrototypeTrim(listGet(list, 'sec-websocket-key'));
    ArrayPrototypePush(fields, ['Sec-WebSocket-Accept', computeAccept(key)]);
  }
  if (protocol !== '') {
    ArrayPrototypePush(fields, ['Sec-WebSocket-Protocol', protocol]);
  }
  if (extensions !== '') {
    ArrayPrototypePush(fields, ['Sec-WebSocket-Extensions', extensions]);
  }

  exchange.commit();
  exchange.isWebSocket = true;
  const transport = exchange.backend.acceptTunnel(exchange, fields, 'websocket');
  const connection = new WebSocketConnection(exchange, transport, {
    pull, deflate, pingInterval: options.pingInterval,
  });
  exchange.tunnel = connection;
  return { connection, protocol, extensions };
}

function toBinary(chunk) {
  if (isArrayBuffer(chunk)) {
    return TypedArrayPrototypeSlice(new Uint8Array(chunk));
  }
  if (isArrayBufferView(chunk)) {
    return TypedArrayPrototypeSlice(new Uint8Array(
      chunk.buffer, chunk.byteOffset, chunk.byteLength));
  }
  return null;
}

// =============================================================================
// ServerWebSocketStream (Section 13)
// =============================================================================

class ServerWebSocketStream {
  #connection = null;
  #opened;
  #closed;
  #readable = null;
  #writable = null;

  constructor(key = undefined, exchange = undefined, options = undefined) {
    if (key !== kConstruct) throw new ERR_ILLEGAL_CONSTRUCTOR();
    if (exchange.state === 'aborted') {
      this.#opened = PromiseReject(exchange.abortReason);
      markHandled(this.#opened);
      this.#closed = PromiseReject(exchange.abortReason);
      markHandled(this.#closed);
      return;
    }
    const { connection, protocol, extensions } =
      performHandshake(exchange, options, true);
    this.#connection = connection;
    this.#closed = connection.closed.promise;

    const { ReadableStream } = require('internal/webstreams/readablestream');
    const { WritableStream } = require('internal/webstreams/writablestream');
    let readableController;
    let writableController;
    this.#readable = new ReadableStream({
      start(controller) {
        readableController = controller;
      },
      async pull(controller) {
        const message = await connection.nextMessage();
        if (message === null) controller.close();
        else controller.enqueue(message);
      },
      cancel(reason) {
        connection.closeWithReason(reason);
      },
    }, { highWaterMark: 0 });
    this.#writable = new WritableStream({
      start(controller) {
        writableController = controller;
      },
      write(chunk) {
        const binary = toBinary(chunk);
        if (binary !== null) return connection.sendMessage(false, binary);
        if (typeof chunk === 'symbol') {
          throw new ERR_INVALID_ARG_TYPE('chunk', 'string', chunk);
        }
        return connection.sendMessage(
          true, encodeUtf8(StringPrototypeToWellFormed(`${chunk}`)));
      },
      close() {
        connection.close(null, '');
      },
      abort(reason) {
        connection.closeWithReason(reason);
      },
    });
    connection.onClean = () => {
      writableController.error(invalidState('The WebSocket connection is closed'));
    };
    connection.onAbnormal = (error) => {
      readableController.error(error);
      writableController.error(error);
    };
    this.#opened = PromiseResolve({
      __proto__: null,
      readable: this.#readable,
      writable: this.#writable,
      extensions,
      protocol,
    });
  }

  #check() {
    if (!(#opened in this)) {
      throw new ERR_INVALID_THIS('ServerWebSocketStream');
    }
  }

  get opened() {
    this.#check();
    return this.#opened;
  }

  get closed() {
    this.#check();
    return this.#closed;
  }

  close(closeInfo = undefined) {
    this.#check();
    const { closeCode, reason } = convertCloseInfo(closeInfo);
    validateServerClose(closeCode, reason);
    this.#connection?.close(closeCode, reason);
  }

  destroy(error = undefined) {
    this.#check();
    this.#connection?.destroy(error);
  }

  [SymbolAsyncDispose]() {
    this.#check();
    const connection = this.#connection;
    if (connection === null || connection.terminated) return PromiseResolve();
    connection.close(null, '');
    return PromisePrototypeThen(connection.closed.promise, () => {});
  }
}

ObjectDefineProperties(ServerWebSocketStream.prototype, {
  opened: kEnumerableProperty,
  closed: kEnumerableProperty,
  close: kEnumerableProperty,
  destroy: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'ServerWebSocketStream',
  },
});

// =============================================================================
// WebSocket (Section 11.4)
// =============================================================================

const kConnecting = 0;
const kOpen = 1;
const kClosing = 2;
const kClosed = 3;

let lazyUndici;
function undici() {
  lazyUndici ??= require('internal/deps/undici/undici');
  return lazyUndici;
}

class WebSocket extends EventTarget {
  #connection = null;
  #url;
  #protocol = '';
  #extensions = '';
  #readyState = kOpen;
  #binaryType = 'blob';
  #bufferedAmount = 0;
  #origin;

  constructor(key = undefined, exchange = undefined, options = undefined) {
    if (key !== kConstruct) throw new ERR_ILLEGAL_CONSTRUCTOR();
    super();
    const url = new URL(exchange.backend.url);
    url.protocol = exchange.backend.encrypted ? 'wss:' : 'ws:';
    this.#url = url.href;
    this.#origin = url.origin;
    if (exchange.state === 'aborted') {
      this.#readyState = kClosed;
      setImmediate(() => this.#fireAbnormal());
      return;
    }
    const { connection, protocol, extensions } =
      performHandshake(exchange, options, false);
    this.#connection = connection;
    this.#protocol = protocol;
    this.#extensions = extensions;
    connection.onMessage = (data) => {
      if (typeof data !== 'string') {
        data = this.#binaryType === 'arraybuffer' ? data.buffer :
          new (require('internal/blob').Blob)([data]);
      }
      const { MessageEvent } = undici();
      this.dispatchEvent(new MessageEvent('message', {
        data, origin: this.#origin,
      }));
    };
    connection.onClosing = () => {
      if (this.#readyState === kOpen) this.#readyState = kClosing;
    };
    connection.onClean = (info) => {
      this.#readyState = kClosed;
      const { CloseEvent } = undici();
      this.dispatchEvent(new CloseEvent('close', {
        wasClean: true, code: info.closeCode, reason: info.reason,
      }));
    };
    connection.onAbnormal = () => {
      this.#readyState = kClosed;
      // Dispatch after the current operation, as a queued task would.
      setImmediate(() => this.#fireAbnormal());
    };
  }

  #fireAbnormal() {
    const { CloseEvent } = undici();
    this.dispatchEvent(new Event('error'));
    this.dispatchEvent(new CloseEvent('close', {
      wasClean: false, code: 1006, reason: '',
    }));
  }

  #check() {
    if (!(#url in this)) throw new ERR_INVALID_THIS('WebSocket');
  }

  get url() {
    this.#check();
    return this.#url;
  }

  get readyState() {
    this.#check();
    return this.#readyState;
  }

  get bufferedAmount() {
    this.#check();
    return this.#bufferedAmount;
  }

  get extensions() {
    this.#check();
    return this.#extensions;
  }

  get protocol() {
    this.#check();
    return this.#protocol;
  }

  get binaryType() {
    this.#check();
    return this.#binaryType;
  }

  set binaryType(value) {
    this.#check();
    if (value === 'blob' || value === 'arraybuffer') this.#binaryType = value;
  }

  close(code = undefined, reason = undefined) {
    this.#check();
    const closeCode = code === undefined ? null :
      converters['unsigned short'](code, { __proto__: null, clamp: true });
    const closeReason = reason === undefined ? '' : converters.USVString(reason);
    validateServerClose(closeCode, closeReason);
    if (this.#readyState === kClosing || this.#readyState === kClosed) return;
    this.#readyState = kClosing;
    this.#connection.closeAfterQueued(closeCode, closeReason);
  }

  send(data) {
    this.#check();
    if (arguments.length === 0) {
      throw new ERR_INVALID_ARG_TYPE('data', 'string', data);
    }
    let text = false;
    let bytes;
    let size;
    const { Blob } = require('internal/blob');
    if (data instanceof Blob) {
      size = data.size;
      bytes = PromisePrototypeThen(data.arrayBuffer(),
                                   (buffer) => new Uint8Array(buffer));
      markHandled(bytes);
    } else {
      bytes = toBinary(data);
      if (bytes === null) {
        if (isAnyArrayBuffer(data)) bytes = TypedArrayPrototypeSlice(new Uint8Array(data));
      }
      if (bytes === null) {
        text = true;
        bytes = encodeUtf8(converters.USVString(data));
      }
      size = bytes.length;
    }
    this.#bufferedAmount += size;
    if (this.#readyState !== kOpen) return;
    const connection = this.#connection;
    if (connection.closeSent || connection.terminated) return;
    markHandled(connection.sendMessage(text, bytes, () => {
      this.#bufferedAmount -= size;
    }));
  }
}

const kStateConstants = {
  CONNECTING: { __proto__: null, value: kConnecting, enumerable: true },
  OPEN: { __proto__: null, value: kOpen, enumerable: true },
  CLOSING: { __proto__: null, value: kClosing, enumerable: true },
  CLOSED: { __proto__: null, value: kClosed, enumerable: true },
};
ObjectDefineProperties(WebSocket, kStateConstants);
ObjectDefineProperties(WebSocket.prototype, {
  ...kStateConstants,
  url: kEnumerableProperty,
  readyState: kEnumerableProperty,
  bufferedAmount: kEnumerableProperty,
  extensions: kEnumerableProperty,
  protocol: kEnumerableProperty,
  binaryType: kEnumerableProperty,
  close: kEnumerableProperty,
  send: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'WebSocket',
  },
});
defineEventHandler(WebSocket.prototype, 'open');
defineEventHandler(WebSocket.prototype, 'error');
defineEventHandler(WebSocket.prototype, 'close');
defineEventHandler(WebSocket.prototype, 'message');

// =============================================================================
// Entry points
// =============================================================================

/**
 * The upgradeWebSocket() steps after the state checks (Section 11.4).
 * @param {import('./exchange').Exchange} exchange
 * @param {object} options Converted WebSocketUpgradeInit.
 * @returns {WebSocket}
 */
function upgradeWebSocket(exchange, options) {
  if (exchange.state !== 'aborted' && !checkHandshake(exchange)) {
    throw new ERR_INVALID_ARG_VALUE(
      'request', 'not a valid WebSocket opening handshake');
  }
  return new WebSocket(kConstruct, exchange, options);
}

/**
 * The upgradeWebSocketStream() steps after the state checks (Section 11.5).
 * @param {import('./exchange').Exchange} exchange
 * @param {object} options Converted WebSocketUpgradeInit.
 * @returns {ServerWebSocketStream}
 */
function upgradeWebSocketStream(exchange, options) {
  if (exchange.state !== 'aborted' && !checkHandshake(exchange)) {
    throw new ERR_INVALID_ARG_VALUE(
      'request', 'not a valid WebSocket opening handshake');
  }
  return new ServerWebSocketStream(kConstruct, exchange, options);
}

module.exports = {
  ServerWebSocketStream,
  WebSocket,
  convertUpgradeOptions,
  upgradeWebSocket,
  upgradeWebSocketStream,
};
