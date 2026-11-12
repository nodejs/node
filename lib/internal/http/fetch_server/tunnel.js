'use strict';

// The Tunnel, CapsuleStream, and DatagramStream of the Fetch Server API.
// https://fetch-server.proposal.wintertc.org/#tunnel

const {
  ArrayPrototypePush,
  ArrayPrototypeShift,
  MathFloor,
  NumberIsSafeInteger,
  ObjectDefineProperties,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  SymbolAsyncDispose,
  SymbolAsyncIterator,
  SymbolToStringTag,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeSet,
  TypedArrayPrototypeSubarray,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  kEnumerableProperty,
} = require('internal/util');

const {
  isUint8Array,
} = require('internal/util/types');

const {
  converters,
} = require('internal/webidl');

const {
  createProtocolError,
  getErrorCode,
  invalidState,
  kConnectError,
  kConnectionReset,
  kH2Codes,
  kTunnelSendable,
  protocolErrorFromH2,
} = require('internal/http/fetch_server/errors');

const {
  markHandled,
} = require('internal/http/fetch_server/body');

const {
  NodeReadableReader,
} = require('internal/http/fetch_server/node_reader');

const {
  drainableProtocol,
} = require('internal/streams/iter/types');

const {
  convertChunks,
  toWriterUint8Array,
} = require('internal/streams/iter/utils');

const kMaxDatagramSize = 65527;
const kDatagramCapsule = 0;

// =============================================================================
// Capsule Protocol codec (RFC 9297 Section 3.2)
// =============================================================================

function varintLength(value) {
  if (value < 64) return 1;
  if (value < 16384) return 2;
  if (value < 1073741824) return 4;
  return 8;
}

function writeVarint(target, offset, value) {
  const length = varintLength(value);
  switch (length) {
    case 1:
      target[offset] = value;
      break;
    case 2:
      target[offset] = 0x40 | (value >>> 8);
      target[offset + 1] = value & 0xff;
      break;
    case 4:
      target[offset] = 0x80 | (value >>> 24);
      target[offset + 1] = (value >>> 16) & 0xff;
      target[offset + 2] = (value >>> 8) & 0xff;
      target[offset + 3] = value & 0xff;
      break;
    default: {
      const high = MathFloor(value / 0x100000000);
      const low = value >>> 0;
      target[offset] = 0xc0 | (high >>> 24);
      target[offset + 1] = (high >>> 16) & 0xff;
      target[offset + 2] = (high >>> 8) & 0xff;
      target[offset + 3] = high & 0xff;
      target[offset + 4] = low >>> 24;
      target[offset + 5] = (low >>> 16) & 0xff;
      target[offset + 6] = (low >>> 8) & 0xff;
      target[offset + 7] = low & 0xff;
    }
  }
  return offset + length;
}

function encodeCapsule(type, data) {
  const dataLength = TypedArrayPrototypeGetByteLength(data);
  const out = new Uint8Array(varintLength(type) + varintLength(dataLength) +
                             dataLength);
  let offset = writeVarint(out, 0, type);
  offset = writeVarint(out, offset, dataLength);
  TypedArrayPrototypeSet(out, data, offset);
  return out;
}

// Reads a varint at offset; returns [value, nextOffset] or null.
function readVarint(buffer, offset) {
  if (offset >= buffer.length) return null;
  const first = buffer[offset];
  const length = 1 << (first >> 6);
  if (offset + length > buffer.length) return null;
  let value = first & 0x3f;
  for (let i = 1; i < length; i++) {
    value = value * 256 + buffer[offset + i];
  }
  return [value, offset + length];
}

class CapsuleParser {
  #buffer = new Uint8Array(0);

  push(chunk) {
    if (this.#buffer.length === 0) {
      this.#buffer = chunk;
    } else {
      const combined = new Uint8Array(this.#buffer.length + chunk.length);
      TypedArrayPrototypeSet(combined, this.#buffer, 0);
      TypedArrayPrototypeSet(combined, chunk, this.#buffer.length);
      this.#buffer = combined;
    }
    const capsules = [];
    for (;;) {
      const type = readVarint(this.#buffer, 0);
      if (type === null) break;
      const length = readVarint(this.#buffer, type[1]);
      if (length === null) break;
      const end = length[1] + length[0];
      if (end > this.#buffer.length) break;
      ArrayPrototypePush(capsules, {
        type: type[0],
        data: TypedArrayPrototypeSubarray(this.#buffer, length[1], end),
      });
      this.#buffer = TypedArrayPrototypeSubarray(this.#buffer, end);
    }
    return capsules;
  }

  get hasPartial() {
    return this.#buffer.length > 0;
  }
}

// =============================================================================
// Tunnel internals
// =============================================================================

class TunnelInternals {
  exchange;
  stream;
  http2;
  connectProtocol;
  mode = 'unclaimed';
  reader;
  readerIterator = null;
  sendDone = false;
  recvDone = false;
  terminated = false;
  closedClean = false;
  error = undefined;
  bytesWritten = 0;
  closed = PromiseWithResolvers();
  endPromise = null;
  pendingDrains = [];
  // Capsule mode.
  capsuleStream = null;
  capsuleParser = null;
  capsuleQueue = [];
  capsuleWaiters = [];
  datagramQueue = [];
  datagramWaiters = [];
  datagramStream = null;
  datagramsEnabled = false;
  capsulesRequested = false;

  constructor(exchange, transport, connectProtocol) {
    this.exchange = exchange;
    this.stream = transport.stream;
    this.http2 = transport.http2;
    this.connectProtocol = connectProtocol;
    this.reader = new NodeReadableReader(this.stream, () => {
      this.recvDone = true;
      this.maybeClose();
    });
    markHandled(this.closed.promise);
    const stream = this.stream;
    stream.on('error', () => {});
    stream.on('drain', () => this.#drained());
    stream.on('close', () => {
      if (this.terminated) return;
      if (this.sendDone && this.recvDone) return;
      this.terminate(this.#closeReason(), false);
    });
  }

  get isWebSocket() {
    return this.connectProtocol === 'websocket';
  }

  #closeReason() {
    if (this.http2) {
      const rstCode = this.stream.rstCode;
      if (rstCode !== undefined && rstCode !== 0) {
        return protocolErrorFromH2(rstCode, 'h2');
      }
    }
    return createProtocolError(kConnectionReset, 'The connection was lost');
  }

  #drained() {
    const drains = this.pendingDrains;
    this.pendingDrains = [];
    for (let i = 0; i < drains.length; i++) drains[i].resolve(true);
  }

  maybeClose() {
    if (this.terminated || !this.sendDone || !this.recvDone) return;
    this.terminated = true;
    this.closedClean = true;
    this.closed.resolve();
    this.exchange.complete();
    if (!this.http2) this.stream.destroy();
  }

  /**
   * Terminates the tunnel abnormally.
   * @param {any} error The error with which the surfaces fail.
   * @param {boolean} local Whether the server terminated the tunnel.
   * @param {string} [code] The error code to send.
   */
  terminate(error, local, code = kConnectError) {
    if (this.terminated) return;
    this.terminated = true;
    this.error = error;
    if (local) {
      if (this.http2) {
        this.stream.close(kH2Codes[code] ?? kH2Codes[kConnectError]);
      } else {
        this.stream.destroy();
      }
    }
    this.#drained();
    this.#wakeCapsules();
    this.#wakeDatagrams();
    this.closed.reject(error);
    if (local) {
      this.exchange.complete();
    } else {
      this.exchange.abort(error);
    }
  }

  // Raw data stream (Section 12.1).

  claimRaw() {
    if (this.mode === 'capsules') {
      throw invalidState('The tunnel data stream carries capsules');
    }
    this.mode = 'raw';
  }

  readRaw() {
    if (this.terminated && !this.closedClean) {
      return PromiseReject(this.error);
    }
    return PromisePrototypeThen(this.reader.read(), undefined, (err) => {
      if (!this.terminated) this.terminate(this.#closeReason(), false);
      throw this.error ?? err;
    });
  }

  write(chunks) {
    if (this.terminated) {
      return PromiseReject(this.error ?? new ERR_INVALID_STATE.TypeError(
        'The tunnel is closed'));
    }
    if (this.endPromise !== null) {
      return PromiseReject(new ERR_INVALID_STATE.TypeError(
        'The tunnel send direction has ended'));
    }
    let ok = true;
    for (let i = 0; i < chunks.length; i++) {
      this.bytesWritten += TypedArrayPrototypeGetByteLength(chunks[i]);
      ok = this.stream.write(chunks[i]);
    }
    if (ok) return PromiseResolve();
    const drain = PromiseWithResolvers();
    ArrayPrototypePush(this.pendingDrains, drain);
    return PromisePrototypeThen(drain.promise, () => {
      if (this.terminated && !this.closedClean) throw this.error;
    });
  }

  end() {
    if (this.terminated && !this.closedClean) {
      return PromiseReject(this.error);
    }
    if (this.endPromise === null) {
      const { promise, resolve, reject } = PromiseWithResolvers();
      this.endPromise = promise;
      markHandled(promise);
      this.stream.end(() => {
        this.sendDone = true;
        resolve(this.bytesWritten);
        this.maybeClose();
      });
      PromisePrototypeThen(this.closed.promise, undefined, reject);
    }
    return this.endPromise;
  }

  // Capsule Protocol (Section 12.2).

  claimCapsules() {
    if (this.mode === 'raw') {
      throw invalidState('The tunnel data stream has been read as bytes');
    }
    if (this.mode === 'capsules') return;
    this.mode = 'capsules';
    this.capsuleParser = new CapsuleParser();
    this.#pumpCapsules();
  }

  async #pumpCapsules() {
    try {
      for (;;) {
        const batch = await this.readRaw();
        if (batch === null) break;
        for (let i = 0; i < batch.length; i++) {
          const capsules = this.capsuleParser.push(batch[i]);
          for (let j = 0; j < capsules.length; j++) {
            const capsule = capsules[j];
            if (capsule.type === kDatagramCapsule) {
              if (this.datagramsEnabled) {
                ArrayPrototypePush(this.datagramQueue, capsule.data);
                this.#wakeDatagrams();
              }
            } else {
              ArrayPrototypePush(this.capsuleQueue, capsule);
              this.#wakeCapsules();
            }
          }
        }
      }
      this.capsuleEnded = true;
    } catch {
      // The tunnel terminated; the error is surfaced by terminate().
    }
    this.#wakeCapsules();
    this.#wakeDatagrams();
  }

  #wakeCapsules() {
    const waiters = this.capsuleWaiters;
    this.capsuleWaiters = [];
    for (let i = 0; i < waiters.length; i++) waiters[i]();
  }

  #wakeDatagrams() {
    const waiters = this.datagramWaiters;
    this.datagramWaiters = [];
    for (let i = 0; i < waiters.length; i++) waiters[i]();
  }

  async nextCapsule() {
    for (;;) {
      if (this.capsuleQueue.length > 0) {
        return ArrayPrototypeShift(this.capsuleQueue);
      }
      if (this.terminated && !this.closedClean) throw this.error;
      if (this.capsuleEnded || this.closedClean) return null;
      const { promise, resolve } = PromiseWithResolvers();
      ArrayPrototypePush(this.capsuleWaiters, resolve);
      await promise;
    }
  }

  async nextDatagrams() {
    for (;;) {
      if (this.datagramQueue.length > 0) {
        const batch = this.datagramQueue;
        this.datagramQueue = [];
        return batch;
      }
      if (this.terminated && !this.closedClean) throw this.error;
      if (this.capsuleEnded || this.closedClean) return null;
      const { promise, resolve } = PromiseWithResolvers();
      ArrayPrototypePush(this.datagramWaiters, resolve);
      await promise;
    }
  }

  sendCapsule(type, data) {
    return this.write([encodeCapsule(type, data)]);
  }

  close() {
    if (this.terminated) {
      return this.closedClean ? PromiseResolve() : PromiseReject(this.error);
    }
    markHandled(this.end());
    return this.closed.promise;
  }

  destroy(error) {
    if (this.terminated) return;
    const code = getErrorCode(error, kTunnelSendable) ?? kConnectError;
    this.terminate(error ?? createProtocolError(kConnectError,
                                                'The tunnel was destroyed'),
                   true, code);
  }
}

// =============================================================================
// Readable and Writer objects
// =============================================================================

function createReadableObject(internals, readStep, onClaim) {
  let reader = null;
  const readable = {
    __proto__: null,
    [SymbolAsyncIterator]() {
      onClaim();
      let done = false;
      const iterator = {
        __proto__: null,
        next() {
          if (iterator !== reader) {
            return PromiseReject(new ERR_INVALID_STATE.TypeError(
              'The tunnel data can be read only once'));
          }
          if (done) return PromiseResolve({ value: undefined, done: true });
          return PromisePrototypeThen(readStep(), (value) => {
            if (value === null) {
              done = true;
              return { value: undefined, done: true };
            }
            return { value, done: false };
          }, (err) => {
            done = true;
            throw err;
          });
        },
        return(value) {
          if (iterator === reader && !done) {
            done = true;
            // Ending iteration early terminates the tunnel (Section 12.1).
            internals.terminate(createProtocolError(
              kConnectError, 'Tunnel iteration ended early'), true);
          }
          return PromiseResolve({ value, done: true });
        },
        [SymbolAsyncIterator]() {
          return this;
        },
      };
      reader ??= iterator;
      return iterator;
    },
  };
  return readable;
}

function createWriter(internals, onClaim, sendStep = (chunks) => internals.write(chunks)) {
  const writer = {
    __proto__: null,
    get canWrite() {
      if (internals.terminated || internals.endPromise !== null) return null;
      return !internals.stream.writableNeedDrain;
    },
    write(chunk, options = undefined) {
      onClaim();
      try {
        chunk = toWriterUint8Array(chunk);
      } catch (err) {
        return PromiseReject(err);
      }
      if (options?.signal?.aborted) return PromiseReject(options.signal.reason);
      return sendStep([chunk]);
    },
    writev(chunks, options = undefined) {
      onClaim();
      try {
        chunks = convertChunks(chunks);
      } catch (err) {
        return PromiseReject(err);
      }
      if (options?.signal?.aborted) return PromiseReject(options.signal.reason);
      return sendStep(chunks);
    },
    writeSync(chunk) {
      onClaim();
      if (writer.canWrite !== true) return false;
      markHandled(sendStep([toWriterUint8Array(chunk)]));
      return true;
    },
    writevSync(chunks) {
      onClaim();
      if (writer.canWrite !== true) return false;
      markHandled(sendStep(convertChunks(chunks)));
      return true;
    },
    end(options = undefined) {
      onClaim();
      if (options?.signal?.aborted) return PromiseReject(options.signal.reason);
      return internals.end();
    },
    endSync() {
      onClaim();
      if (internals.terminated) return -1;
      markHandled(internals.end());
      return -1;
    },
    fail(reason = undefined) {
      onClaim();
      if (internals.terminated) return;
      const code = getErrorCode(reason, kTunnelSendable) ?? kConnectError;
      internals.terminate(reason ?? createProtocolError(
        kConnectError, 'The tunnel writer failed'), true, code);
    },
    [drainableProtocol]() {
      const canWrite = writer.canWrite;
      if (canWrite === null) return null;
      if (canWrite) return PromiseResolve(true);
      const drain = PromiseWithResolvers();
      ArrayPrototypePush(internals.pendingDrains, drain);
      return drain.promise;
    },
    [SymbolAsyncDispose]() {
      if (internals.endPromise !== null || internals.terminated) {
        return PromiseResolve();
      }
      writer.fail();
      return PromiseResolve();
    },
  };
  return writer;
}

// =============================================================================
// CapsuleStream (Section 5.10)
// =============================================================================

class CapsuleStream {
  #internals;
  #readable;

  constructor(key, internals) {
    if (key !== kConstruct) {
      throw new ERR_INVALID_THIS('CapsuleStream cannot be constructed');
    }
    this.#internals = internals;
    this.#readable = createReadableObject(internals,
                                          () => internals.nextCapsule(),
                                          () => {});
  }

  get readable() {
    if (this.#internals === undefined) throw new ERR_INVALID_THIS('CapsuleStream');
    return this.#readable;
  }

  send(capsule) {
    const internals = this.#internals;
    if (internals === undefined) {
      return PromiseReject(new ERR_INVALID_THIS('CapsuleStream'));
    }
    if (capsule === null || typeof capsule !== 'object') {
      return PromiseReject(new ERR_INVALID_ARG_TYPE('capsule', 'Object', capsule));
    }
    let type;
    try {
      type = converters['long long'](capsule.type, {
        __proto__: null, enforceRange: true, context: 'capsule.type',
      });
    } catch (err) {
      return PromiseReject(err);
    }
    const data = capsule.data;
    if (!isUint8Array(data)) {
      return PromiseReject(new ERR_INVALID_ARG_TYPE('capsule.data', 'Uint8Array',
                                                    data));
    }
    if (type < 0 || !NumberIsSafeInteger(type)) {
      return PromiseReject(new ERR_INVALID_ARG_VALUE('capsule.type', type));
    }
    if (type === kDatagramCapsule) {
      return PromiseReject(new ERR_INVALID_ARG_VALUE(
        'capsule.type', type, 'DATAGRAM capsules are sent with datagrams()'));
    }
    return internals.sendCapsule(type, data);
  }
}

ObjectDefineProperties(CapsuleStream.prototype, {
  readable: kEnumerableProperty,
  send: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'CapsuleStream',
  },
});

// =============================================================================
// Tunnel (Section 5.9)
// =============================================================================

const kConstruct = {};
let getTunnelInternals;
let isTunnel;

class Tunnel {
  #internals;
  #readable = null;
  #writer = null;

  static {
    getTunnelInternals = (tunnel) => tunnel.#internals;
    isTunnel = (value) => {
      return value != null && typeof value === 'object' && #internals in value;
    };
  }

  constructor(key, internals) {
    if (key !== kConstruct) {
      throw new ERR_INVALID_THIS('Tunnel cannot be constructed');
    }
    this.#internals = internals;
  }

  get readable() {
    const internals = checkTunnel(this);
    if (internals.mode === 'capsules') {
      throw invalidState('The tunnel data stream carries capsules');
    }
    this.#readable ??= createReadableObject(
      internals, () => internals.readRaw(), () => internals.claimRaw());
    return this.#readable;
  }

  get writer() {
    const internals = checkTunnel(this);
    if (internals.mode === 'capsules') {
      throw invalidState('The tunnel data stream carries capsules');
    }
    this.#writer ??= createWriter(internals, () => internals.claimRaw());
    return this.#writer;
  }

  capsules() {
    const internals = checkTunnel(this);
    if (internals.connectProtocol === null || internals.isWebSocket) {
      throw invalidState(
        'The Capsule Protocol is not available on this tunnel');
    }
    if (internals.mode === 'raw') {
      throw invalidState('The tunnel data stream has been read as bytes');
    }
    if (internals.capsulesRequested) {
      throw invalidState('capsules() has already been called');
    }
    internals.capsulesRequested = true;
    internals.claimCapsules();
    internals.capsuleStream = new CapsuleStream(kConstruct, internals);
    return internals.capsuleStream;
  }

  datagrams() {
    const internals = checkTunnel(this);
    if (internals.connectProtocol === null || internals.isWebSocket) {
      throw invalidState('HTTP Datagrams are not available on this tunnel');
    }
    if (internals.mode === 'raw') {
      throw invalidState('The tunnel data stream has been read as bytes');
    }
    internals.claimCapsules();
    if (internals.datagramStream === null) {
      internals.datagramsEnabled = true;
      const readable = createReadableObject(
        internals, () => internals.nextDatagrams(), () => {});
      // Ending iteration of datagrams early does not affect the tunnel.
      const iteratorFactory = readable[SymbolAsyncIterator];
      readable[SymbolAsyncIterator] = function() {
        const iterator = iteratorFactory();
        iterator.return = (value) => {
          internals.datagramsEnabled = false;
          internals.datagramQueue = [];
          return PromiseResolve({ value, done: true });
        };
        return iterator;
      };
      let ended = false;
      const writer = createWriter(internals, () => {}, (chunks) => {
        if (ended) {
          return PromiseReject(new ERR_INVALID_STATE.TypeError(
            'The datagram writer has ended'));
        }
        const capsules = [];
        for (let i = 0; i < chunks.length; i++) {
          const size = TypedArrayPrototypeGetByteLength(chunks[i]);
          if (size <= kMaxDatagramSize) {
            ArrayPrototypePush(capsules, encodeCapsule(kDatagramCapsule,
                                                       chunks[i]));
          }
        }
        return internals.write(capsules);
      });
      writer.end = () => {
        ended = true;
        return PromiseResolve(0);
      };
      writer.fail = () => {
        ended = true;
      };
      internals.datagramStream = { readable, writer };
    }
    return {
      readable: internals.datagramStream.readable,
      writer: internals.datagramStream.writer,
      unreliable: false,
      maxDatagramSize: kMaxDatagramSize,
    };
  }

  close() {
    try {
      return checkTunnel(this).close();
    } catch (err) {
      return PromiseReject(err);
    }
  }

  destroy(error = undefined) {
    checkTunnel(this).destroy(error);
  }

  get closed() {
    return checkTunnel(this).closed.promise;
  }

  [SymbolAsyncDispose]() {
    const internals = checkTunnel(this);
    if (internals.terminated) return PromiseResolve();
    return internals.close();
  }
}

function checkTunnel(self) {
  if (!isTunnel(self)) throw new ERR_INVALID_THIS('Tunnel');
  return getTunnelInternals(self);
}

ObjectDefineProperties(Tunnel.prototype, {
  readable: kEnumerableProperty,
  writer: kEnumerableProperty,
  capsules: kEnumerableProperty,
  datagrams: kEnumerableProperty,
  close: kEnumerableProperty,
  destroy: kEnumerableProperty,
  closed: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'Tunnel',
  },
});

/**
 * Creates a Tunnel for an accepted exchange.
 * @returns {Tunnel}
 */
function createTunnel(exchange, transport, connectProtocol) {
  const internals = new TunnelInternals(exchange, transport, connectProtocol);
  exchange.tunnel = internals;
  return new Tunnel(kConstruct, internals);
}

module.exports = {
  Tunnel,
  createTunnel,
};
