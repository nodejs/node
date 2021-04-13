'use strict';

const {
  ObjectDefineProperties,
  Symbol,
} = primordials;

const { Buffer } = require('buffer');

const {
  ConfigObject,
  OptionsObject,
  RandomConnectionIDStrategy,
  createClientSecureContext,
  createServerSecureContext,
  HTTP3_ALPN,
  NGTCP2_CC_ALGO_CUBIC,
  NGTCP2_CC_ALGO_RENO,
  NGTCP2_MAX_CIDLEN,
  NGTCP2_PREFERRED_ADDRESS_USE,
  NGTCP2_PREFERRED_ADDRESS_IGNORE,
} = internalBinding('quic');

// If the HTTP3_ALPN is undefined, the Node.js binary
// was built without QUIC support, in which case we
// don't want to export anything here.
if (HTTP3_ALPN === undefined)
  return;

const {
  SocketAddress,
  kHandle: kSocketAddressHandle,
} = require('internal/socketaddress');

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const {
  Http3Options,
  kHandle: kHttp3OptionsHandle,
} = require('internal/quic/http3');

const {
  kEnumerableProperty,
} = require('internal/webstreams/util');

const {
  isArrayBufferView,
  isAnyArrayBuffer,
} = require('internal/util/types');

const {
  configSecureContext,
} = require('internal/tls/secure-context');

const {
  inspect,
} = require('util');

const {
  codes: {
    ERR_MISSING_OPTION,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_THIS,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const {
  validateBigIntOrSafeInteger,
  validateBoolean,
  validateInt32,
  validateNumber,
  validateObject,
  validatePort,
  validateString,
  validateUint32,
} = require('internal/validators');

const kHandle = Symbol('kHandle');
const kSide = Symbol('kSide');
const kOptions = Symbol('kOptions');
const kSecureContext = Symbol('kSecureContext');
const kGetPreferredAddress = Symbol('kGetPreferredAddress');
const kGetTransportParams = Symbol('kGetTransportParams');
const kGetSecureOptions = Symbol('kGetSecureOptions');
const kGetApplicationOptions = Symbol('kGetApplicationOptions');
const kType = Symbol('kType');

const kResetTokenSecretLen = 16;

const kDefaultQuicCiphers = 'TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:' +
                            'TLS_CHACHA20_POLY1305_SHA256';
const kDefaultGroups = 'P-256:X25519:P-384:P-521';

const kRandomConnectionIdStrategy = new RandomConnectionIDStrategy();

/**
 *
 * @typedef { import('../socketaddress').SocketAddressOrOptions
 * } SocketAddressOrOptions
 *
 * @typedef {{
 *   address? : SocketAddressOrOptions,
 *   retryTokenExpiration? : number | bigint,
 *   tokenExpiration? : number | bigint,
 *   maxWindowOverride? : number | bigint,
 *   maxStreamWindowOverride? : number | bigint,
 *   maxConnectionsPerHost? : number | bigint,
 *   maxConnectionsTotal? : number | bigint,
 *   maxStatelessResets? : number | bigint,
 *   addressLRUSize? : number | bigint,
 *   retryLimit? : number | bigint,
 *   maxPayloadSize? : number | bigint,
 *   unacknowledgedPacketThreshold? : number | bigint,
 *   validateAddress? : boolean,
 *   disableStatelessReset? : boolean,
 *   rxPacketLoss? : number,
 *   txPacketLoss? : number,
 *   ccAlgorithm? : 'reno' | 'cubic',
 *   udp? : {
 *     ipv6Only? : boolean,
 *     receiveBufferSize? : number,
 *     sendBufferSize? : number,
 *     ttl? : number
 *   },
 *   resetTokenSecret? : ArrayBuffer | TypedArrray | DataView
 * }} EndpointConfigOptions
 *
 * @typedef {{
 *   initialMaxStreamDataBidiLocal? : number | bigint
 *   initialMaxStreamDataBidiRemote? : number | bigint
 *   initialMaxStreamDataUni? : number | bigint
 *   initialMaxData? : number | bigint
 *   initialMaxStreamsBidi? : number | bigint
 *   initialMaxStreamsUni? : number | bigint
 *   maxIdleTimeout? : number | bigint
 *   activeConnectionIdLimit? : number | bigint
 *   ackDelayExponent? : number | bigint
 *   maxAckDelay? : number | bigint
 *   maxDatagramFrameSize? : number | bigint
 *   disableActiveMigration? : boolean,
 *   preferredAddress? : {
 *     ipv4? : SocketAddressOrOptions,
 *     ipv6? : SocketAddressOrOptions
 *   }
 * }} TransportParams
 *
 * @typedef {{
 *   ca? : any,
 *   cert? : any,
 *   sigalgs? : string,
 *   ciphers? : string,
 *   clientCertEngine? : string,
 *   crl? : any,
 *   dhparam? : any,
 *   ecdhCurve? : string,
 *   key? : any,
 *   ocsp? : boolean,
 *   privateKey? : {
 *     engine: string,
 *     identifier: string
 *   },
 *   passphrase? : any,
 *   pfx? : any,
 *   secureOptions? : number,
 *   sessionIdContext? : string,
 *   ticketKeys? : any,
 *   sessionTimeout? : number,
 *   clientHello? : boolean,
 *   enableTLSTrace? : boolean,
 *   keylog? : boolean,
 *   pskCallback? : Function<void>,
 *   rejectUnauthorized? : boolean,
 *   requestPeerCertificate? : boolean,
 *   verifyHostnameIdentity? : boolean
 * }} SecureOptions
 *
 * @typedef {{
 *   alpn? : string,
 *   dcid? : string,
 *   scid? : string,
 *   hostname? : string,
 *   preferredAddressStrategy? : 'use'|'ignore',
 *   qlog? boolean,
 *   transportParams? : TransportParams,
 *   secure? : SecureOptions,
 * }} SessionConfigOptions
 *
 * @typedef {EndpointConfig | EndpointConfigOptions} EndpointConfigOrOptions
 * @typedef {SessionConfig | SessionConfigOptions} SessionConfigOrOptions
 * @typedef {import('../blob.js').Blob} Blob
 * @typedef {import('stream').Readable} Readable
 * @typedef {ArrayBuffer | TypedArray | DataView | Blob | Readable | string
 * } StreamPayload
 *
 * @typedef {{
 *   unidirectional? : boolean,
 *   headers? : Object | Map<string,string>,
 *   trailers? : Object | Map<string,string>,
 *   body? : StreamPayload | Promise<StreamPayload>,
 * }} StreamOptionsInit
 *
 * @typedef {{
 *   headers? : Object | Map<string,string>,
 *   trailers? : Object | Map<string, string>,
 *   body? : StreamPayload | Promise<StreamPayload>,
 * }} ResponseOptionsInit
 */
class EndpointConfig {
  /**
   * @param {*} val
   * @returns {boolean}
   */
  static isEndpointConfig(val) {
    return typeof val?.[kOptions] === 'object' &&
      val?.[kType] === 'EndpointConfig';
  }

  /**
   * @param {EndpointConfigOptions} options
   */
  constructor(options = {}) {
    this[kType] = 'EndpointConfig';
    validateObject(options, 'options');
    let { address = new SocketAddress({ address: '127.0.0.1' }) } = options;
    const {
      retryTokenExpiration,
      tokenExpiration,
      maxWindowOverride,
      maxStreamWindowOverride,
      maxConnectionsPerHost,
      maxConnectionsTotal,
      maxStatelessResets,
      addressLRUSize,
      retryLimit,
      maxPayloadSize,
      unacknowledgedPacketThreshold,
      validateAddress,
      disableStatelessReset,
      rxPacketLoss,
      txPacketLoss,
      ccAlgorithm,
      udp,
      resetTokenSecret,
    } = options;

    if (!SocketAddress.isSocketAddress(address)) {
      if (address == null || typeof address !== 'object') {
        throw new ERR_INVALID_ARG_TYPE(
          'options.address',
          ['SocketAddress', 'Object'],
          address);
      }
      const {
        address: _address = '127.0.0.1',
        port = 0,
        family = 'ipv4',
      } = address;
      validateString(_address, 'options.address.address');
      validatePort(port, 'options.address.port');
      validateString(family, 'options.address.family');
      address = new SocketAddress({ address: _address, port, family });
    }

    if (retryTokenExpiration !== undefined) {
      validateBigIntOrSafeInteger(
        retryTokenExpiration,
        'options.retryTokenExpiration');
    }

    if (tokenExpiration !== undefined) {
      validateBigIntOrSafeInteger(
        tokenExpiration,
        'options.tokenExpiration');
    }

    if (maxWindowOverride !== undefined) {
      validateBigIntOrSafeInteger(
        maxWindowOverride,
        'options.maxWindowOverride');
    }

    if (maxStreamWindowOverride !== undefined) {
      validateBigIntOrSafeInteger(
        maxStreamWindowOverride,
        'options.maxStreamWindowOverride');
    }

    if (maxConnectionsPerHost !== undefined) {
      validateBigIntOrSafeInteger(
        maxConnectionsPerHost,
        'options.maxConnectionsPerHost');
    }

    if (maxConnectionsTotal !== undefined) {
      validateBigIntOrSafeInteger(
        maxConnectionsTotal,
        'options.maxConnectionsTotal');
    }

    if (maxStatelessResets !== undefined) {
      validateBigIntOrSafeInteger(
        maxStatelessResets,
        'options.maxStatelessResets');
    }

    if (addressLRUSize !== undefined) {
      validateBigIntOrSafeInteger(
        addressLRUSize,
        'options.addressLRUSize');
    }

    if (retryLimit !== undefined) {
      validateBigIntOrSafeInteger(
        retryLimit,
        'options.retryLimit');
    }

    if (maxPayloadSize !== undefined) {
      validateBigIntOrSafeInteger(
        maxPayloadSize,
        'options.mayPayloadSize');
    }

    if (unacknowledgedPacketThreshold !== undefined) {
      validateBigIntOrSafeInteger(
        unacknowledgedPacketThreshold,
        'options.unacknowledgedPacketThreshold');
    }

    if (validateAddress !== undefined)
      validateBoolean(validateAddress, 'options.validateAddress');

    if (disableStatelessReset !== undefined)
      validateBoolean(disableStatelessReset, 'options.disableStatelessReset');

    if (rxPacketLoss !== undefined) {
      validateNumber(rxPacketLoss, 'options.rxPacketLoss');
      if (rxPacketLoss < 0.0 || rxPacketLoss > 1.0) {
        throw new ERR_OUT_OF_RANGE(
          'options.rxPacketLoss',
          'between 0.0 and 1.0',
          rxPacketLoss);
      }
    }

    if (txPacketLoss !== undefined) {
      validateNumber(txPacketLoss, 'options.txPacketLoss');
      if (txPacketLoss < 0.0 || txPacketLoss > 1.0) {
        throw new ERR_OUT_OF_RANGE(
          'config.txPacketLoss',
          'between 0.0 and 1.0',
          txPacketLoss);
      }
    }

    let ccAlgo;
    if (ccAlgorithm !== undefined) {
      validateString(ccAlgorithm, 'options.ccAlgorithm');
      switch (ccAlgorithm) {
        case 'cubic': ccAlgo = NGTCP2_CC_ALGO_CUBIC; break;
        case 'reno': ccAlgo = NGTCP2_CC_ALGO_RENO; break;
        default:
          throw new ERR_INVALID_ARG_VALUE(
            'options.ccAlgorithm',
            ccAlgorithm,
            'be either `cubic` or `reno`');
      }
    }

    if (udp !== undefined)
      validateObject(udp, 'options.udp');

    const {
      ipv6Only = false,
      receiveBufferSize = 0,
      sendBufferSize = 0,
      ttl = 0,
    } = udp || {};
    validateBoolean(ipv6Only, 'options.udp.ipv6Only');
    if (receiveBufferSize !== undefined)
      validateUint32(receiveBufferSize, 'options.udp.receiveBufferSize');
    if (sendBufferSize !== undefined)
      validateUint32(sendBufferSize, 'options.udp.sendBufferSize');
    if (ttl !== undefined)
      validateInt32(ttl, 'options.udp.ttl', 0, 255);

    if (resetTokenSecret !== undefined) {
      if (!isAnyArrayBuffer(resetTokenSecret) &&
          !isArrayBufferView(resetTokenSecret)) {
        throw new ERR_INVALID_ARG_TYPE('options.resetTokenSecret', [
          'ArrayBuffer',
          'Buffer',
          'TypedArray',
          'DataView',
        ], resetTokenSecret);
      }
      if (resetTokenSecret.byteLength !== kResetTokenSecretLen) {
        throw new ERR_INVALID_ARG_VALUE(
          'options.resetTokenSecret',
          resetTokenSecret.byteLength,
          `be exactly ${kResetTokenSecretLen} bytes long`);
      }
    }

    this[kOptions] = {
      address,
      retryTokenExpiration,
      tokenExpiration,
      maxWindowOverride,
      maxStreamWindowOverride,
      maxConnectionsPerHost,
      maxConnectionsTotal,
      maxStatelessResets,
      addressLRUSize,
      retryLimit,
      maxPayloadSize,
      unacknowledgedPacketThreshold,
      validateAddress,
      disableStatelessReset,
      rxPacketLoss,
      txPacketLoss,
      ccAlgorithm,
      ipv6Only,
      receiveBufferSize,
      sendBufferSize,
      ttl,
      resetTokenSecret: resetTokenSecret || '(generated)',
    };

    this[kHandle] = new ConfigObject(
      address[kSocketAddressHandle],
      {
        retryTokenExpiration,
        tokenExpiration,
        maxWindowOverride,
        maxStreamWindowOverride,
        maxConnectionsPerHost,
        maxConnectionsTotal,
        maxStatelessResets,
        addressLRUSize,
        retryLimit,
        maxPayloadSize,
        unacknowledgedPacketThreshold,
        validateAddress,
        disableStatelessReset,
        rxPacketLoss,
        txPacketLoss,
        ccAlgorithm: ccAlgo,
        ipv6Only,
        receiveBufferSize,
        sendBufferSize,
        ttl,
      });

    if (resetTokenSecret !== undefined) {
      this[kHandle].setResetTokenSecret(resetTokenSecret);
    } else {
      this[kHandle].generateResetTokenSecret();
    }
  }

  [kInspect](depth, options) {
    if (!EndpointConfig.isEndpointConfig(this))
      throw new ERR_INVALID_THIS('EndpointConfig');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `${this[kType]} ${inspect(this[kOptions], opts)}`;
  }
}

function validateCID(cid, name) {
  if (cid === undefined)
    return cid;

  if (typeof cid === 'string')
    cid = Buffer.from(cid, 'hex');

  if (!isArrayBufferView(cid) && !isAnyArrayBuffer(cid)) {
    throw new ERR_INVALID_ARG_TYPE(
      name,
      [
        'string',
        'ArrayBuffer',
        'Buffer',
        'TypedArray',
        'DataView',
      ],
      cid);
  }

  if (cid.byteLength > NGTCP2_MAX_CIDLEN) {
    throw new ERR_INVALID_ARG_VALUE(
      name,
      cid.byteLength,
      `be no more than ${NGTCP2_MAX_CIDLEN} bytes in length`);
  }

  return cid;
}

class SessionConfig {
  /**
   * @param {*} val
   * @returns {boolean}
   */
  static isSessionConfig(val) {
    return typeof val?.[kOptions] === 'object' &&
      val?.[kType] === 'SessionConfig';
  }

  /**
   * @param {string} side - One of either 'client' or 'server'
   * @param {SessionConfigOptions} [options]
   */
  constructor(side, options = {}) {
    this[kType] = 'SessionConfig';
    validateString(side, 'side');
    validateObject(options, 'options');
    const {
      alpn = HTTP3_ALPN,
      hostname,
      preferredAddressStrategy,
      qlog,
      transportParams,
      secure,
      application,
    } = options;
    let {
      dcid,
      scid,
    } = options;

    switch (side) {
      case 'client':
        this[kSide] = 'client';
        break;
      case 'server':
        this[kSide] = 'server';
        break;
      default:
        throw new ERR_INVALID_ARG_VALUE(
          'side',
          side,
          'be either `client` or `server`.');
    }

    validateString(alpn, 'options.alpn');
    if (alpn.length > 255) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.alpn',
        alpn,
        '<= 255 characters in length');
    }

    if (hostname !== undefined)
      validateString(hostname, 'options.hostname');

    dcid = validateCID(dcid, 'options.dcid');
    scid = validateCID(scid, 'options.scid');

    let pas;
    if (preferredAddressStrategy !== undefined) {
      validateString(
        preferredAddressStrategy,
        'options.preferredAddressStrategy');
      switch (preferredAddressStrategy) {
        case 'use':
          pas = NGTCP2_PREFERRED_ADDRESS_USE;
          break;
        case 'ignore':
          pas = NGTCP2_PREFERRED_ADDRESS_IGNORE;
          break;
        default:
          throw new ERR_INVALID_ARG_VALUE(
            'options.preferredAddressStrategy',
            preferredAddressStrategy,
            'be either `use` or `ignore`.');
      }
    }

    if (qlog !== undefined)
      validateBoolean(qlog, 'options.qlog');

    this[kOptions] = {
      alpn,
      hostname,
      dcid,
      scid,
      preferredAddressStrategy,
      qlog,
    };

    this[kGetApplicationOptions](alpn, application);

    this[kGetSecureOptions](secure);

    this[kSecureContext] = side === 'server' ?
      createServerSecureContext() :
      createClientSecureContext();

    configSecureContext(this[kSecureContext], this[kOptions].secure);

    this[kHandle] = new OptionsObject(
      alpn,
      hostname,
      dcid,
      scid,
      pas,
      kRandomConnectionIdStrategy,
      qlog,
      this[kOptions].secure,
      this[kOptions].application[kHttp3OptionsHandle],
      ...this[kGetTransportParams](transportParams));
  }

  /**
   * @readonly
   * @type {string}
   */
  get side() {
    if (!SessionConfig.isSessionConfig(this))
      throw new ERR_INVALID_THIS('SessionConfig');
    return this[kSide];
  }

  /**
   * @readonly
   * @type {string}
   */
  get hostname() {
    if (!SessionConfig.isSessionConfig(this))
      throw new ERR_INVALID_THIS('SessionConfig');
    return this[kOptions].hostname;
  }

  [kGetApplicationOptions](alpn, options = {}) {
    validateObject(options, 'options.application');
    switch (alpn) {
      case HTTP3_ALPN:
        this[kOptions].application =
            Http3Options.isHttp3Options(options) ?
              options :
              new Http3Options(options);
        break;
      default:
        this[kOptions].application = options;
        break;
    }
  }

  [kGetPreferredAddress](addr, name, family) {
    if (!SocketAddress.isSocketAddress(addr)) {
      validateObject(addr, name);
      const {
        address,
        port
      } = addr;
      addr = new SocketAddress({ address, port, family });
    }
    if (addr.family !== family) {
      throw new ERR_INVALID_ARG_VALUE(
        `${name}.family`,
        addr.family,
        `must be ${family}`);
    }
    return addr[kSocketAddressHandle];
  }

  [kGetTransportParams](params) {
    if (params === undefined) return [, , , ];
    validateObject(params, 'options.transportParams');
    const {
      initialMaxStreamDataBidiLocal,
      initialMaxStreamDataBidiRemote,
      initialMaxStreamDataUni,
      initialMaxData,
      initialMaxStreamsBidi,
      initialMaxStreamsUni,
      maxIdleTimeout,
      activeConnectionIdLimit,
      ackDelayExponent,
      maxAckDelay,
      maxDatagramFrameSize,
      disableActiveMigration,
      preferredAddress: {
        ipv4,
        ipv6
      } = {},
    } = params;

    if (initialMaxStreamDataBidiLocal !== undefined) {
      validateBigIntOrSafeInteger(
        initialMaxStreamDataBidiLocal,
        'options.transportParams.initialMaxStreamDataBidiLocal');
    }

    if (initialMaxStreamDataBidiRemote !== undefined) {
      validateBigIntOrSafeInteger(
        initialMaxStreamDataBidiRemote,
        'options.transportParams.initialMaxStreamDataBidiRemote');
    }

    if (initialMaxStreamDataUni !== undefined) {
      validateBigIntOrSafeInteger(
        initialMaxStreamDataUni,
        'options.transportParams.initialMaxStreamDataUni');
    }

    if (initialMaxData !== undefined) {
      validateBigIntOrSafeInteger(
        initialMaxData,
        'options.transportParams.initialMaxData');
    }

    if (initialMaxStreamsBidi !== undefined) {
      validateBigIntOrSafeInteger(
        initialMaxStreamsBidi,
        'options.transportParams.initialMaxStreamsBidi');
    }

    if (initialMaxStreamsUni !== undefined) {
      validateBigIntOrSafeInteger(
        initialMaxStreamsUni,
        'options.transportParams.initialMaxStreamsUni');
    }

    if (maxIdleTimeout !== undefined) {
      validateBigIntOrSafeInteger(
        maxIdleTimeout,
        'options.transportParams.maxIdleTimeout');
    }

    if (activeConnectionIdLimit !== undefined) {
      validateBigIntOrSafeInteger(
        activeConnectionIdLimit,
        'options.transportParams.activeConnectionIdLimit');
    }

    if (ackDelayExponent !== undefined) {
      validateBigIntOrSafeInteger(
        ackDelayExponent,
        'options.transportParams.ackDelayExponent');
    }

    if (maxAckDelay !== undefined) {
      validateBigIntOrSafeInteger(
        maxAckDelay,
        'options.transportParams.maxAckDelay');
    }

    if (maxDatagramFrameSize !== undefined) {
      validateBigIntOrSafeInteger(
        maxDatagramFrameSize,
        'options.transportParams.maxDatagramFrameSize');
    }

    if (disableActiveMigration !== undefined) {
      validateBoolean(
        disableActiveMigration,
        'options.transportParams.disableActiveMigration');
    }

    const ipv4PreferredAddress = ipv4 !== undefined ?
      this[kGetPreferredAddress](
        ipv4,
        'options.transportParams.preferredAddress.ipv4',
        'ipv4') : undefined;
    const ipv6PreferredAddress = ipv6 !== undefined ?
      this[kGetPreferredAddress](
        ipv6,
        'options.transportParams.preferredAddress.ipv6',
        'ipv6') : undefined;

    this[kOptions].transportParams = {
      initialMaxStreamDataBidiLocal,
      initialMaxStreamDataBidiRemote,
      initialMaxStreamDataUni,
      initialMaxData,
      initialMaxStreamsBidi,
      initialMaxStreamsUni,
      maxIdleTimeout,
      activeConnectionIdLimit,
      ackDelayExponent,
      maxAckDelay,
      maxDatagramFrameSize,
      disableActiveMigration,
      preferredAddress: {
        ipv4,
        ipv6,
      },
    };

    return [
      this[kOptions].transportParams,
      ipv4PreferredAddress,
      ipv6PreferredAddress,
    ];
  }

  [kGetSecureOptions](options = {}) {
    validateObject(options, 'options.secure');
    const {
      // Secure context options
      ca,
      cert,
      sigalgs,
      ciphers = kDefaultQuicCiphers,
      clientCertEngine,
      crl,
      dhparam,
      ecdhCurve,
      groups = kDefaultGroups,
      key,
      privateKey: {
        engine: privateKeyEngine,
        identifier: privateKeyIdentifier,
      } = {},
      passphrase,
      pfx,
      secureOptions,
      sessionIdContext = 'node.js quic server',
      ticketKeys,
      sessionTimeout,

      clientHello,
      enableTLSTrace,
      keylog,
      pskCallback,
      rejectUnauthorized,
      ocsp,
      requestPeerCertificate,
      verifyHostnameIdentity
    } = options;

    if (clientHello !== undefined)
      validateBoolean(clientHello, 'options.secure.clientHello');

    if (enableTLSTrace !== undefined)
      validateBoolean(enableTLSTrace, 'options.secure.enableTLSTrace');

    if (keylog !== undefined)
      validateBoolean(keylog, 'options.secure.keylog');

    if (pskCallback !== undefined && typeof pskCallback !== 'function') {
      throw new ERR_INVALID_ARG_TYPE(
        'options.secure.pskCallback',
        'function',
        pskCallback);
    }

    if (rejectUnauthorized !== undefined) {
      validateBoolean(
        rejectUnauthorized,
        'options.secure.rejectUnauthorized');
    }

    if (requestPeerCertificate !== undefined) {
      validateBoolean(
        requestPeerCertificate,
        'options.secure.requestPeerCertificate');
    }

    if (ocsp !== undefined)
      validateBoolean(ocsp, 'options.secure.ocsp');

    if (verifyHostnameIdentity !== undefined) {
      validateBoolean(
        verifyHostnameIdentity,
        'options.secure.verifyHostnameIdentity');
    }

    this[kOptions].secure = {
      ca,
      cert,
      sigalgs,
      ciphers,
      clientCertEngine,
      crl,
      dhparam,
      ecdhCurve,
      groups,
      key,
      privateKeyEngine,
      privateKeyIdentifier,
      passphrase,
      pfx,
      secureOptions,
      sessionIdContext,
      ticketKeys,
      sessionTimeout,
      clientHello,
      enableTLSTrace,
      keylog,
      pskCallback,
      rejectUnauthorized,
      ocsp,
      requestPeerCertificate,
      verifyHostnameIdentity,
    };

    return this[kOptions].secure;
  }

  [kInspect](depth, options) {
    if (!SessionConfig.isSessionConfig(this))
      throw new ERR_INVALID_THIS('SessionConfig');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `${this[kType]} ${inspect(this[kOptions], opts)}`;
  }
}

ObjectDefineProperties(SessionConfig.prototype, {
  side: kEnumerableProperty,
  hostname: kEnumerableProperty,
});

class StreamOptions {
  /**
   * @param {*} val
   * @returns {boolean}
   */
  static isStreamOptions(val) {
    return typeof val?.[kOptions] === 'object' &&
      val?.[kType] === 'StreamOptions';
  }

  /**
   * @param {StreamOptionsInit} [options]
   */
  constructor(options = {}) {
    this[kType] = 'StreamOptions';
    validateObject(options, 'options');
    const {
      unidirectional = false,
      headers,
      trailers,
      body,
    } = options;

    validateBoolean(unidirectional, 'options.unidirectional');

    // Validation of headers, trailers, and body will happen later.
    this[kOptions] = {
      unidirectional,
      headers,
      trailers,
      body,
    };
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get unidirectional() {
    if (!StreamOptions.isStreamOptions(this))
      throw new ERR_INVALID_THIS('StreamOptions');
    return this[kOptions].unidirectional;
  }

  /**
   * @readonly
   */
  get headers() {
    if (!StreamOptions.isStreamOptions(this))
      throw new ERR_INVALID_THIS('StreamOptions');
    return this[kOptions].headers;
  }

  /**
   * @readonly
   */
  get trailers() {
    if (!StreamOptions.isStreamOptions(this))
      throw new ERR_INVALID_THIS('StreamOptions');
    return this[kOptions].trailers;
  }

  /**
   * @readonly
   * @type {Promise<StreamPayload>}
   */
  get body() {
    if (!StreamOptions.isStreamOptions(this))
      throw new ERR_INVALID_THIS('StreamOptions');
    return this[kOptions].body;
  }

  [kInspect](depth, options) {
    if (!StreamOptions.isStreamOptions(this))
      throw new ERR_INVALID_THIS('StreamOptions');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `${this[kType]} ${inspect(this[kOptions], opts)}`;
  }
}

ObjectDefineProperties(StreamOptions.prototype, {
  unidirectional: kEnumerableProperty,
  headers: kEnumerableProperty,
  trailers: kEnumerableProperty,
  body: kEnumerableProperty,
});

class ResponseOptions {
  /**
   * @param {*} val
   * @returns {boolean}
   */
  static isResponseOptions(val) {
    return typeof val?.[kOptions] === 'object' &&
      val?.[kType] === 'ResponseOptions';
  }

  /**
   * @param {ResponseOptionsInit} [options]
   */
  constructor(options = {}) {
    this[kType] = 'ResponseOptions';
    validateObject(options, 'options');
    const {
      hints,
      headers,
      trailers,
      body,
    } = options;

    this[kOptions] = {
      hints,
      headers,
      trailers,
      body,
    };
  }

  /**
   * @readonly
   * @type {{}}
   */
  get hints() {
    if (!ResponseOptions.isResponseOptions(this))
      throw new ERR_INVALID_THIS('ResponseOptions');
    return this[kOptions].hints;
  }

  /**
   * @readonly
   * @type {{} | Promise<{}>}
   */
  get headers() {
    if (!ResponseOptions.isResponseOptions(this))
      throw new ERR_INVALID_THIS('ResponseOptions');
    return this[kOptions].headers;
  }

  /**
   * @readonly`
   * @type {{} | Promise<{}>}
   */
  get trailers() {
    if (!ResponseOptions.isResponseOptions(this))
      throw new ERR_INVALID_THIS('ResponseOptions');
    return this[kOptions].trailers;
  }

  /**
   * @readonly
   * @type {Promise<StreamPayload>}
   */
  get body() {
    if (!ResponseOptions.isResponseOptions(this))
      throw new ERR_INVALID_THIS('ResponseOptions');
    return this[kOptions].body;
  }

  [kInspect](depth, options) {
    if (!ResponseOptions.isResponseOptions(this))
      throw new ERR_INVALID_THIS('ResponseOptions');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `${this[kType]} ${inspect(this[kOptions], opts)}`;
  }
}

ObjectDefineProperties(ResponseOptions.prototype, {
  hints: kEnumerableProperty,
  headers: kEnumerableProperty,
  trailers: kEnumerableProperty,
  body: kEnumerableProperty,
});

/**
 * @param {ArrayBuffer | TypedArray | DataView} sessionTicket
 * @param {ArrayBuffer | TypedArray | DataView} transportParams
 * @returns {void}
 */
function validateResumeOptions(sessionTicket, transportParams) {
  if (sessionTicket !== undefined && transportParams === undefined) {
    throw new ERR_MISSING_OPTION(
      'if options.sessionTicket is provided, options.transportParams');
  }

  if (sessionTicket === undefined && transportParams !== undefined) {
    throw new ERR_MISSING_OPTION(
      'if options.transportParams is provided, options.sessionTicket');
  }

  if (sessionTicket !== undefined &&
      !isAnyArrayBuffer(sessionTicket) &&
      !isArrayBufferView(sessionTicket)) {
    throw new ERR_INVALID_ARG_TYPE(
      'resume.sessionTicket', [
        'ArrayBuffer',
        'TypedArray',
        'DataView',
        'Buffer',
      ],
      sessionTicket);
  }

  if (transportParams !== undefined &&
      !isAnyArrayBuffer(transportParams) &&
      !isArrayBufferView(transportParams)) {
    throw new ERR_INVALID_ARG_TYPE(
      'resume.transportParams', [
        'ArrayBuffer',
        'TypedArray',
        'DataView',
        'Buffer',
      ],
      transportParams);
  }
}

module.exports = {
  EndpointConfig,
  SessionConfig,
  StreamOptions,
  ResponseOptions,
  validateResumeOptions,
  kSecureContext,
  kHandle,
  // Exported for testing purposes only
  kOptions,
};
