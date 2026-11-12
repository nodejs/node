'use strict';

const {
  PromiseReject,
} = primordials;

const {
  getOptionValue,
} = require('internal/options');

if (!process.features.quic || !getOptionValue('--experimental-quic')) {
  return;
}

// Internal, experimental HTTP/3 layer over node:quic.
//
// An Http3Session is started on a QuicConnection that has no session yet,
// and from then on the connection's streams are HTTP/3 request streams.

const {
  QuicSessionBase,
  createApplicationStream,
  getApplicationCallback,
  getSessionConnection,
  isServerConnection,
  setApplicationCallback,
} = require('internal/quic/quic');

const {
  kPrivateConstructor,
} = require('internal/quic/symbols');
const { kEmptyObject } = require('internal/util');

const {
  QUIC_APPLICATION_HTTP3,
  STREAM_DIRECTION_BIDIRECTIONAL: kStreamDirectionBidirectional,
} = internalBinding('quic');

const {
  validateBigIntOrInt,
  validateBoolean,
  validateFunction,
  validateObject,
} = require('internal/validators');

const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

const kMaxUint64 = (1n << 64n) - 1n;

function validateUint64Setting(value, name) {
  if (value === undefined) return undefined;
  return validateBigIntOrInt(value, `options.settings.${name}`, 0n, kMaxUint64);
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

class Http3Session extends QuicSessionBase {
  /**
   * Starts HTTP/3 on a QuicConnection that has no session yet.
   * @param {QuicConnection} connection
   * @param {object} [options]
   * @param {ApplicationOptions} [options.settings]
   * @param {Function} [options.ongoaway]
   * @param {Function} [options.onorigin]
   * @param {Function} [options.onsettings]
   * @returns {Http3Session}
   */
  static start(connection, options) {
    return new Http3Session(kPrivateConstructor, connection, options);
  }

  constructor(privateSymbol, connection, options = kEmptyObject) {
    validateObject(options, 'options');
    const { ongoaway, onorigin, onsettings, settings } = options;
    if (ongoaway !== undefined) validateFunction(ongoaway, 'options.ongoaway');
    if (onorigin !== undefined) validateFunction(onorigin, 'options.onorigin');
    if (onsettings !== undefined) {
      validateFunction(onsettings, 'options.onsettings');
    }
    let preparedSettings;
    if (settings !== undefined) {
      validateObject(settings, 'options.settings');
      preparedSettings = prepareH3Settings(settings);
    }

    // Reading settings could call getters and go into JS, so start after:
    super(privateSymbol, connection, QUIC_APPLICATION_HTTP3, preparedSettings);

    // User code could have patched setters, so we set directly:
    if (ongoaway !== undefined) {
      setApplicationCallback(connection, 'ongoaway', ongoaway, this);
    }
    if (onorigin !== undefined) {
      setApplicationCallback(connection, 'onorigin', onorigin, this);
    }
    if (onsettings !== undefined) {
      setApplicationCallback(connection, 'onapplication', onsettings, this, 'onsettings');
    }
  }

  /**
   * The settings in effect, including any update from the peer's SETTINGS
   * frame, which may arrive after the session opens. Null once destroyed.
   * @type {ApplicationOptions|null}
   */
  get settings() { return getSessionConnection(this).applicationOptions; }

  /** @type {Function|undefined} */
  get ongoaway() { return getApplicationCallback(getSessionConnection(this), 'ongoaway'); }
  set ongoaway(fn) { setApplicationCallback(getSessionConnection(this), 'ongoaway', fn, this); }

  /** @type {Function|undefined} */
  get onorigin() { return getApplicationCallback(getSessionConnection(this), 'onorigin'); }
  set onorigin(fn) { setApplicationCallback(getSessionConnection(this), 'onorigin', fn, this); }

  /** @type {Function|undefined} */
  get onsettings() { return getApplicationCallback(getSessionConnection(this), 'onapplication'); }
  set onsettings(fn) {
    setApplicationCallback(getSessionConnection(this), 'onapplication', fn, this, 'onsettings');
  }

  /**
   * Opens a request stream.
   * @returns {Promise<QuicStream>}
   */
  createBidirectionalStream(options) {
    if (isServerConnection(getSessionConnection(this))) {
      return PromiseReject(new ERR_INVALID_STATE(
        'Server sessions cannot open HTTP/3 request streams'));
    }
    return createApplicationStream(
      getSessionConnection(this), kStreamDirectionBidirectional, options);
  }
}

module.exports = {
  Http3Session,
};
