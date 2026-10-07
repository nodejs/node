'use strict';

const {
  BigInt,
  NumberIsInteger,
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
  validateBoolean,
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

    if (ongoaway !== undefined) this.ongoaway = ongoaway;
    if (onorigin !== undefined) this.onorigin = onorigin;
    if (onsettings !== undefined) this.onsettings = onsettings;
  }

  /**
   * The settings in effect, including any update from the peer's SETTINGS
   * frame, which may arrive after the session opens. Null once destroyed.
   * @type {ApplicationOptions|null}
   */
  get settings() { return this.connection.applicationOptions; }

  /** @type {Function|undefined} */
  get ongoaway() { return getApplicationCallback(this.connection, 'ongoaway'); }
  set ongoaway(fn) { setApplicationCallback(this.connection, 'ongoaway', fn, this); }

  /** @type {Function|undefined} */
  get onorigin() { return getApplicationCallback(this.connection, 'onorigin'); }
  set onorigin(fn) { setApplicationCallback(this.connection, 'onorigin', fn, this); }

  /** @type {Function|undefined} */
  get onsettings() { return getApplicationCallback(this.connection, 'onapplication'); }
  set onsettings(fn) {
    setApplicationCallback(this.connection, 'onapplication', fn, this, 'onsettings');
  }

  /**
   * Opens a request stream.
   * @returns {Promise<QuicStream>}
   */
  createBidirectionalStream(options) {
    if (isServerConnection(this.connection)) {
      return PromiseReject(new ERR_INVALID_STATE(
        'Server sessions cannot open HTTP/3 request streams'));
    }
    return createApplicationStream(
      this.connection, kStreamDirectionBidirectional, options);
  }
}

module.exports = {
  Http3Session,
};
