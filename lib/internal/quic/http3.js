'use strict';

const {
  BigInt,
  FunctionPrototypeBind,
  NumberIsInteger,
  SymbolAsyncDispose,
} = primordials;

const {
  getOptionValue,
} = require('internal/options');

if (!process.features.quic || !getOptionValue('--experimental-quic')) {
  return;
}

// Internal, experimental HTTP/3 layer over node:quic.
//
// A QuicSession created without an application is a raw QUIC session.
// Attaching an Http3Session makes it an HTTP/3 one, and from then on its
// streams are HTTP/3 request streams.

const {
  createApplicationStream,
  getQuicSessionHandle,
  getQuicSessionState,
  isQuicSession,
  setApplicationCallback,
} = require('internal/quic/quic');

const {
  kInspect,
  kPrivateConstructor,
} = require('internal/quic/symbols');
const { kEmptyObject } = require('internal/util');
const { inspect } = require('internal/util/inspect');

const {
  QUIC_APPLICATION_HTTP3,
  STREAM_DIRECTION_BIDIRECTIONAL: kStreamDirectionBidirectional,
  kHttp3Settings,
} = internalBinding('quic');

const {
  validateBoolean,
  validateFunction,
  validateObject,
} = require('internal/validators');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const kMaxUint64 = (1n << 64n) - 1n;

function validateUint64Setting(value, name) {
  if (value === undefined) return undefined;
  let big;
  if (typeof value === 'bigint') {
    big = value;
  } else if (typeof value === 'number') {
    if (!NumberIsInteger(value)) {
      throw new ERR_OUT_OF_RANGE(`options.settings.${name}`, 'an integer',
                                 value);
    }
    big = BigInt(value);
  } else {
    throw new ERR_INVALID_ARG_TYPE(`options.settings.${name}`,
                                   ['number', 'bigint'], value);
  }
  if (big < 0n || big > kMaxUint64) {
    throw new ERR_OUT_OF_RANGE(`options.settings.${name}`,
                               `>= 0 && <= ${kMaxUint64}`, value);
  }
  return big;
}

function validateBooleanSetting(value, name) {
  if (value !== undefined) validateBoolean(value, `options.settings.${name}`);
  return value;
}

// Read each setting just once (they may be getters), then validate and return
// them as a plain null-proto object:
function prepareH3Settings(settings) {
  const {
    maxHeaderPairs,
    maxHeaderLength,
    maxFieldSectionSize,
    qpackMaxDTableCapacity,
    qpackEncoderMaxDTableCapacity,
    qpackBlockedStreams,
    enableConnectProtocol,
    enableDatagrams,
  } = settings;
  return {
    __proto__: null,
    maxHeaderPairs: validateUint64Setting(maxHeaderPairs, 'maxHeaderPairs'),
    maxHeaderLength: validateUint64Setting(maxHeaderLength, 'maxHeaderLength'),
    maxFieldSectionSize:
      validateUint64Setting(maxFieldSectionSize, 'maxFieldSectionSize'),
    qpackMaxDTableCapacity:
      validateUint64Setting(qpackMaxDTableCapacity, 'qpackMaxDTableCapacity'),
    qpackEncoderMaxDTableCapacity: validateUint64Setting(
      qpackEncoderMaxDTableCapacity, 'qpackEncoderMaxDTableCapacity'),
    qpackBlockedStreams:
      validateUint64Setting(qpackBlockedStreams, 'qpackBlockedStreams'),
    enableConnectProtocol:
      validateBooleanSetting(enableConnectProtocol, 'enableConnectProtocol'),
    enableDatagrams: validateBooleanSetting(enableDatagrams, 'enableDatagrams'),
  };
}

function checkAttachable(session, state) {
  if (session.destroyed) {
    throw new ERR_INVALID_STATE(
      'An application cannot be attached to a destroyed QUIC session');
  }
  if (state.applicationType !== 0) {
    throw new ERR_INVALID_STATE(
      'The QUIC session already has an application attached');
  }
}

class Http3Session {
  #session;
  #onstream;
  #ongoaway;
  #onorigin;
  #onsettings;
  #onerror;

  /**
   * Attaches HTTP/3 to a QuicSession that has not yet become active.
   * @param {QuicSession} session the QUIC session to attach to
   * @param {object} [options]
   * @param {ApplicationOptions} [options.settings]
   * @param {Function} [options.ongoaway]
   * @param {Function} [options.onorigin]
   * @param {Function} [options.onsettings]
   * @returns {Http3Session}
   */
  static from(session, options) {
    return new Http3Session(kPrivateConstructor, session, options);
  }

  constructor(privateSymbol, session, options = kEmptyObject) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    if (!isQuicSession(session)) {
      throw new ERR_INVALID_ARG_TYPE('session', 'QuicSession', session);
    }
    validateObject(options, 'options');
    const { ongoaway, onorigin, onsettings, settings } = options;
    if (ongoaway !== undefined) validateFunction(ongoaway, 'options.ongoaway');
    if (onorigin !== undefined) validateFunction(onorigin, 'options.onorigin');
    if (onsettings !== undefined) {
      validateFunction(onsettings, 'options.onsettings');
    }
    const state = getQuicSessionState(session);

    let preparedSettings;
    if (settings !== undefined) {
      validateObject(settings, 'options.settings');
      preparedSettings = prepareH3Settings(settings);
    }

    // Reading settings could call getters and go into JS, so do this after:
    checkAttachable(session, state);

    const handle = getQuicSessionHandle(session);
    if (preparedSettings !== undefined) handle[kHttp3Settings] = preparedSettings;
    state.applicationType = QUIC_APPLICATION_HTTP3;
    this.#session = session;
    if (ongoaway !== undefined) {
      this.#ongoaway = ongoaway;
      setApplicationCallback(session, 'ongoaway', this.#bind(ongoaway));
    }
    if (onorigin !== undefined) {
      this.#onorigin = onorigin;
      setApplicationCallback(session, 'onorigin', this.#bind(onorigin));
    }
    if (onsettings !== undefined) {
      this.#onsettings = onsettings;
      setApplicationCallback(session, 'onapplication', this.#bind(onsettings));
    }
  }

  /**
   * The QUIC session carrying this HTTP/3 session
   * @type {QuicSession}
   */
  get quicSession() { return this.#session; }

  /**
   * The settings in effect, including any update from the peer's SETTINGS
   * frame, which may arrive after the session opens. Null once destroyed.
   * @type {ApplicationOptions|null}
   */
  get settings() { return this.#session.applicationOptions; }

  /** @type {quic.QuicSession.Stats} */
  get stats() { return this.#session.stats; }

  // Ensure that 'this' in callbacks registered here is the Http3Session
  #bind(fn) {
    return fn === undefined ? undefined : FunctionPrototypeBind(fn, this);
  }

  /**
   * Called with each request stream the client opens. HTTP/3 has no
   * server-initiated requests, so this never fires on a client session.
   * @type {Function|undefined}
   */
  get onstream() { return this.#onstream; }
  set onstream(fn) {
    if (fn !== undefined) validateFunction(fn, 'onstream');
    setApplicationCallback(this.#session, 'onstream', this.#bind(fn));
    this.#onstream = fn;
  }

  /** @type {Function|undefined} */
  get ongoaway() { return this.#ongoaway; }
  set ongoaway(fn) {
    if (fn !== undefined) validateFunction(fn, 'ongoaway');
    setApplicationCallback(this.#session, 'ongoaway', this.#bind(fn));
    this.#ongoaway = fn;
  }

  /** @type {Function|undefined} */
  get onorigin() { return this.#onorigin; }
  set onorigin(fn) {
    if (fn !== undefined) validateFunction(fn, 'onorigin');
    setApplicationCallback(this.#session, 'onorigin', this.#bind(fn));
    this.#onorigin = fn;
  }

  /** @type {Function|undefined} */
  get onsettings() { return this.#onsettings; }
  set onsettings(fn) {
    if (fn !== undefined) validateFunction(fn, 'onsettings');
    setApplicationCallback(this.#session, 'onapplication', this.#bind(fn));
    this.#onsettings = fn;
  }

  /**
   * Called with a session error after the QUIC session's own onerror, which
   * stays the transport-level handler. Either may be set independently.
   * @type {Function|undefined}
   */
  get onerror() { return this.#onerror; }
  set onerror(fn) {
    if (fn !== undefined) validateFunction(fn, 'onerror');
    setApplicationCallback(this.#session, 'onapperror', this.#bind(fn));
    this.#onerror = fn;
  }

  /** @type {Promise<object>} */
  get opened() { return this.#session.opened; }

  /** @type {Promise<void>} */
  get closed() { return this.#session.closed; }

  /** @type {boolean} */
  get closing() { return this.#session.closing; }

  /** @type {boolean} */
  get destroyed() { return this.#session.destroyed; }

  /**
   * Opens a request stream.
   * @returns {Promise<QuicStream>}
   */
  async createBidirectionalStream(options) {
    if (getQuicSessionState(this.#session).isServer) {
      throw new ERR_INVALID_STATE(
        'Server sessions cannot open HTTP/3 request streams');
    }
    return await createApplicationStream(
      this.#session, kStreamDirectionBidirectional, options);
  }

  close(options) { return this.#session.close(options); }

  destroy(error, options) { return this.#session.destroy(error, options); }

  async [SymbolAsyncDispose]() { await this.close(); }

  [kInspect](depth, options) {
    if (depth < 0) {
      return 'Http3Session { }';
    }
    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };
    return `Http3Session ${inspect({
      quicSession: this.#session,
      settings: this.settings,
      destroyed: this.destroyed,
    }, opts)}`;
  }
}

module.exports = {
  Http3Session,
};
