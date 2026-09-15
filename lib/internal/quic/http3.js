'use strict';

const {
  BigInt,
  FunctionPrototypeBind,
  NumberIsInteger,
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
  QuicSession,
  getQuicSessionState,
  kCreateStream,
  kSessionHandle,
} = require('internal/quic/quic');

const {
  QUIC_APPLICATION_HTTP3,
  QUIC_APPLICATION_PENDING,
  STREAM_DIRECTION_BIDIRECTIONAL: kStreamDirectionBidirectional,
  kHttp3Settings,
} = internalBinding('quic');

const {
  validateFunction,
  validateObject,
} = require('internal/validators');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const kEmptyObject = { __proto__: null };
const kMaxUint64 = (1n << 64n) - 1n;

const kNumericSettings = [
  'maxHeaderPairs',
  'maxHeaderLength',
  'maxFieldSectionSize',
  'qpackMaxDTableCapacity',
  'qpackEncoderMaxDTableCapacity',
  'qpackBlockedStreams',
];
const kBooleanSettings = [
  'enableConnectProtocol',
  'enableDatagrams',
];

// Validate settings and return them as a plain null-proto object:
function prepareH3Settings(settings) {
  const out = { __proto__: null };
  for (let n = 0; n < kNumericSettings.length; n++) {
    const name = kNumericSettings[n];
    const value = settings[name];
    if (value === undefined) continue;
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
    out[name] = big;
  }
  for (let n = 0; n < kBooleanSettings.length; n++) {
    const name = kBooleanSettings[n];
    const value = settings[name];
    if (value === undefined) continue;
    if (typeof value !== 'boolean') {
      throw new ERR_INVALID_ARG_TYPE(`options.settings.${name}`, 'boolean',
                                     value);
    }
    out[name] = value;
  }
  return out;
}

function checkAttachable(session, state) {
  if (session.destroyed ||
    state.applicationType === undefined ||
    session[kSessionHandle] === undefined) {
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
   */
  constructor(session, options = kEmptyObject) {
    if (!(session instanceof QuicSession)) {
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

    const handle = session[kSessionHandle];
    if (preparedSettings !== undefined) handle[kHttp3Settings] = preparedSettings;
    state.applicationType = QUIC_APPLICATION_HTTP3 | QUIC_APPLICATION_PENDING;
    this.#session = session;
    if (ongoaway !== undefined) this.ongoaway = ongoaway;
    if (onorigin !== undefined) this.onorigin = onorigin;
    if (onsettings !== undefined) this.onsettings = onsettings;
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

  /** @type {string|undefined} */
  get servername() { return this.#session.servername; }

  /** @type {string|undefined} */
  get alpnProtocol() { return this.#session.alpnProtocol; }

  /** @type {object|undefined} */
  get certificate() { return this.#session.certificate; }

  /** @type {object|undefined} */
  get peerCertificate() { return this.#session.peerCertificate; }

  /** @type {object|undefined} */
  get ephemeralKeyInfo() { return this.#session.ephemeralKeyInfo; }

  /** @type {quic.QuicSession.Stats} */
  get stats() { return this.#session.stats; }

  // Ensure that 'this' in callbacks registered here is the Http3Session
  #bind(fn) {
    return fn === undefined ? undefined : FunctionPrototypeBind(fn, this);
  }

  /**
   * Called with each request stream the peer opens.
   * @type {Function|undefined}
   */
  get onstream() { return this.#onstream; }
  set onstream(fn) {
    this.#session.onstream = this.#bind(fn);
    this.#onstream = fn;
  }

  /** @type {Function|undefined} */
  get ongoaway() { return this.#ongoaway; }
  set ongoaway(fn) {
    this.#session.ongoaway = this.#bind(fn);
    this.#ongoaway = fn;
  }

  /** @type {Function|undefined} */
  get onorigin() { return this.#onorigin; }
  set onorigin(fn) {
    this.#session.onorigin = this.#bind(fn);
    this.#onorigin = fn;
  }

  /** @type {Function|undefined} */
  get onsettings() { return this.#onsettings; }
  set onsettings(fn) {
    this.#session.onapplication = this.#bind(fn);
    this.#onsettings = fn;
  }

  /** @type {Function|undefined} */
  get onerror() { return this.#onerror; }
  set onerror(fn) {
    this.#session.onerror = this.#bind(fn);
    this.#onerror = fn;
  }

  /** @type {Promise<object>} */
  get opened() { return this.#session.opened; }

  /** @type {Promise<void>} */
  get closed() { return this.#session.closed; }

  /** @type {boolean} */
  get destroyed() { return this.#session.destroyed; }

  /**
   * Opens a request stream.
   * @returns {Promise<QuicStream>}
   */
  createBidirectionalStream(options) {
    if (getQuicSessionState(this.#session).isServer) {
      throw new ERR_INVALID_STATE(
        'Server sessions cannot open HTTP/3 request streams');
    }
    return this.#session[kCreateStream](
      kStreamDirectionBidirectional, options);
  }

  close(options) { return this.#session.close(options); }

  destroy(error, options) { return this.#session.destroy(error, options); }
}

module.exports = {
  Http3Session,
};
