'use strict';

// TODO(@jasnell) Temporarily ignoring c8 coverage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  ArrayIsArray,
  FunctionPrototypeBind,
  PromisePrototypeThen,
  PromiseWithResolvers,
  SafeSet,
  SymbolAsyncDispose,
} = primordials;

const {
  getOptionValue,
} = require('internal/options');

// DTLS requires that Node.js be compiled with crypto support.
if (!process.features.dtls || !getOptionValue('--experimental-dtls')) {
  return;
}

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
    ERR_MISSING_ARGS,
  },
} = require('internal/errors');

const {
  validateFunction,
  validateObject,
  validateString,
  validateInteger,
} = require('internal/validators');

const {
  Buffer,
} = require('buffer');

const {
  isIP,
} = require('internal/net');

const {
  DTLSEndpointState,
  DTLSSessionState,
} = require('internal/dtls/state');

const {
  DTLSEndpointStats,
  DTLSSessionStats,
} = require('internal/dtls/stats');

const {
  kOwner,
  kPrivateConstructor,
  kSessionHandshake,
  kSessionMessage,
  kSessionError,
  kSessionClose,
  kSessionKeylog,
} = require('internal/dtls/symbols');

const {
  DTLSContext: DTLSContext_,
  DTLSEndpoint: DTLSEndpoint_,
  SSL_VERIFY_NONE_VALUE,
  SSL_VERIFY_PEER_VALUE,
  SSL_VERIFY_FAIL_IF_NO_PEER_CERT_VALUE,
} = internalBinding('dtls');

const kEmptyObject = { __proto__: null };

// ============================================================================
// DTLSSession -- represents a single DTLS peer association
// ============================================================================

class DTLSSession {
  #handle;
  #endpoint;
  #state;
  #stats;
  #pendingOpen;
  #pendingClose;
  #onmessage;
  #onerror;
  #onhandshake;
  #onkeylog;
  #ownsEndpoint = false;

  constructor(privateSymbol, handle, endpoint) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }

    this.#handle = handle;
    this.#handle[kOwner] = this;
    this.#endpoint = endpoint;
    this.#state = new DTLSSessionState(
      kPrivateConstructor, handle.getState());
    this.#stats = new DTLSSessionStats(
      kPrivateConstructor, handle.getStats());
    this.#pendingOpen = PromiseWithResolvers();
    this.#pendingClose = PromiseWithResolvers();
    // opened/closed may reject (handshake error, destroy(error)). Attach a
    // no-op rejection handler so a caller that uses the callback API and never
    // awaits them does not trigger an unhandled rejection; an explicit
    // await/then/catch on opened/closed still observes the rejection.
    PromisePrototypeThen(this.#pendingOpen.promise, undefined, () => {});
    PromisePrototypeThen(this.#pendingClose.promise, undefined, () => {});
  }

  // --- Callback setters ---

  set onmessage(fn) {
    if (fn !== undefined && fn !== null) {
      validateFunction(fn, 'onmessage');
      this.#onmessage = FunctionPrototypeBind(fn, this);
      this.#state.hasMessageListener = true;
    } else {
      this.#onmessage = undefined;
      this.#state.hasMessageListener = false;
    }
  }

  get onmessage() { return this.#onmessage; }

  set onerror(fn) {
    if (fn !== undefined && fn !== null) {
      validateFunction(fn, 'onerror');
      this.#onerror = FunctionPrototypeBind(fn, this);
    } else {
      this.#onerror = undefined;
    }
  }

  get onerror() { return this.#onerror; }

  set onhandshake(fn) {
    if (fn !== undefined && fn !== null) {
      validateFunction(fn, 'onhandshake');
      this.#onhandshake = FunctionPrototypeBind(fn, this);
    } else {
      this.#onhandshake = undefined;
    }
  }

  get onhandshake() { return this.#onhandshake; }

  set onkeylog(fn) {
    if (fn !== undefined && fn !== null) {
      validateFunction(fn, 'onkeylog');
      this.#onkeylog = FunctionPrototypeBind(fn, this);
    } else {
      this.#onkeylog = undefined;
    }
  }

  get onkeylog() { return this.#onkeylog; }

  // --- Send data ---

  send(data) {
    if (this.#handle === null) {
      throw new ERR_INVALID_STATE('Session is destroyed');
    }
    if (typeof data === 'string') {
      data = Buffer.from(data);
    }
    if (!Buffer.isBuffer(data)) {
      throw new ERR_INVALID_ARG_TYPE('data', ['string', 'Buffer'], data);
    }
    return this.#handle.send(data);
  }

  // --- Lifecycle ---

  close() {
    if (this.#handle === null) return this.closed;
    const handle = this.#handle;
    this.#handle = null;
    handle.close();
    return this.closed;
  }

  destroy(error) {
    if (this.#handle === null) return;
    const handle = this.#handle;
    this.#handle = null;
    handle.destroy();
    if (error) {
      this.#pendingClose.reject(error);
    } else {
      this.#pendingClose.resolve();
    }
  }

  get opened() { return this.#pendingOpen.promise; }
  get closed() { return this.#pendingClose.promise; }

  // --- Properties ---

  get remoteAddress() {
    if (this.#handle === null) return undefined;
    return this.#handle.getRemoteAddress();
  }

  get protocol() {
    if (this.#handle === null) return undefined;
    return this.#handle.getProtocol();
  }

  get cipher() {
    if (this.#handle === null) return undefined;
    return this.#handle.getCipher();
  }

  get peerCertificate() {
    if (this.#handle === null) return undefined;
    return this.#handle.getPeerCertificate();
  }

  get alpnProtocol() {
    if (this.#handle === null) return undefined;
    return this.#handle.getALPNProtocol();
  }

  get srtpProfile() {
    if (this.#handle === null) return undefined;
    return this.#handle.getSRTPProfile();
  }

  get servername() {
    if (this.#handle === null) return undefined;
    return this.#handle.getServername();
  }

  get state() { return this.#state; }
  get stats() { return this.#stats; }
  get endpoint() { return this.#endpoint; }

  exportKeyingMaterial(length, label, context) {
    if (this.#handle === null) {
      throw new ERR_INVALID_STATE('Session is destroyed');
    }
    return this.#handle.exportKeyingMaterial(length, label, context);
  }

  // --- Internal callbacks (called from C++ via endpoint dispatch) ---

  [kSessionHandshake](protocol) {
    this.#pendingOpen.resolve({ protocol });
    if (this.#onhandshake) {
      this.#onhandshake(protocol);
    }
  }

  [kSessionMessage](data) {
    if (this.#onmessage) {
      this.#onmessage(data);
    }
  }

  [kSessionError](message) {
    const error = new ERR_INVALID_STATE(message);
    if (this.#onerror) {
      this.#onerror(error);
    }
    this.#pendingOpen.reject(error);

    // The session has failed and cannot continue. Tear it down so it does not
    // linger in the endpoint's table, and -- for a client session that owns
    // its internal endpoint -- close the endpoint too so the event loop can
    // drain. destroy() removes the session from the C++ table first, so the
    // endpoint.close() below won't try to re-close it. Reentrant destroy from
    // within the error emit is safe: Cycle()/the timer hold a strong ref.
    const endpoint = this.#endpoint;
    const ownsEndpoint = this.#ownsEndpoint;
    this.destroy();
    if (endpoint) {
      endpoint.sessions.delete(this);
      if (ownsEndpoint) {
        endpoint.close();
      }
    }
  }

  [kSessionClose]() {
    this.#pendingClose.resolve();
    this.#handle = null;
    // Remove from the endpoint's JS-side session set.
    if (this.#endpoint) {
      this.#endpoint.sessions.delete(this);
    }
    // If this session owns its endpoint (client-side connect()),
    // close the endpoint too so the process can exit.
    if (this.#ownsEndpoint && this.#endpoint) {
      this.#endpoint.close();
    }
  }

  // Mark that this session owns its endpoint (for client sessions
  // created by connect() where the endpoint is internal).
  get ownsEndpoint() { return this.#ownsEndpoint; }
  set ownsEndpoint(val) { this.#ownsEndpoint = val; }

  [kSessionKeylog](line) {
    if (this.#onkeylog) {
      this.#onkeylog(line);
    }
  }

  async [SymbolAsyncDispose]() {
    await this.close();
  }
}

// ============================================================================
// DTLSEndpoint -- manages a UDP socket and routes datagrams to sessions
// ============================================================================

class DTLSEndpoint {
  #handle;
  #state;
  #stats;
  #sessions = new SafeSet();
  #pendingClose;
  #onsession;
  #onerror;

  constructor(options = kEmptyObject) {
    this.#handle = new DTLSEndpoint_();
    this.#handle[kOwner] = this;
    this.#state = new DTLSEndpointState(
      kPrivateConstructor, this.#handle.getState());
    this.#stats = new DTLSEndpointStats(
      kPrivateConstructor, this.#handle.getStats());
    this.#pendingClose = PromiseWithResolvers();
    // See DTLSSession: keep an unobserved closed rejection from surfacing as an
    // unhandled rejection.
    PromisePrototypeThen(this.#pendingClose.promise, undefined, () => {});

    if (options.mtu !== undefined) {
      validateInteger(options.mtu, 'options.mtu', 256, 65535);
      this.#handle.setMTU(options.mtu);
    }

    // Set up the callback dispatch from C++ to JS.
    this.#handle.setCallbacks({
      __proto__: null,
      onEndpointClose: () => this.#onEndpointClose(),
      onEndpointError: (msg) => this.#onEndpointError(msg),
      onSessionNew: (handle) => this.#onSessionNew(handle),
      onSessionClose: function() {
        this[kOwner]?.[kSessionClose]();
      },
      onSessionError: function(msg) {
        this[kOwner]?.[kSessionError](msg);
      },
      onSessionHandshake: function(protocol) {
        this[kOwner]?.[kSessionHandshake](protocol);
      },
      onSessionMessage: function(data) {
        this[kOwner]?.[kSessionMessage](data);
      },
      onSessionKeylog: function(line) {
        this[kOwner]?.[kSessionKeylog](line);
      },
      onSessionTicket: function() {
        // Session ticket handling - placeholder for resumption.
      },
    });
  }

  // --- Server mode ---

  listen(callback, context) {
    validateFunction(callback, 'callback');
    this.#onsession = callback;
    this.#handle.listen(context);
    return this;
  }

  // --- Client mode ---

  connect(context, host, port, servername) {
    // Resolve SNI and the expected peer identity here so that every caller of
    // the endpoint API -- not only the top-level dtls.connect() -- gets safe
    // defaults. The identity is always bound to the requested servername (or,
    // failing that, the host). OpenSSL only *enforces* it when the context is
    // in a verifying mode, so binding it is a no-op for non-verifying
    // (rejectUnauthorized: false) contexts.
    //
    // These are applied to the client SSL inside the binding, before the
    // handshake's ClientHello is emitted; they cannot be set afterwards.
    let sni = servername !== undefined ? (servername || undefined) : host;
    if (sni !== undefined && isIP(sni) !== 0) {
      sni = undefined;  // SNI is never sent for IP literals (matching TLS).
    }
    const verifyHost = servername || host;
    const verifyIsIp = isIP(verifyHost) !== 0;

    const sessionHandle = this.#handle.connect(
      context, host, port, sni, verifyHost, verifyIsIp);
    const session = new DTLSSession(
      kPrivateConstructor, sessionHandle, this);
    this.#sessions.add(session);
    return session;
  }

  // --- Bind ---

  bind(host, port) {
    this.#handle.bind(host, port);
    return this;
  }

  // --- Lifecycle ---

  close() {
    if (this.#handle === null) return this.closed;
    const handle = this.#handle;
    this.#handle = null;
    handle.close();
    return this.closed;
  }

  destroy(error) {
    if (this.#handle === null) return;
    const handle = this.#handle;
    this.#handle = null;
    handle.destroy();
    if (error) {
      this.#pendingClose.reject(error);
    } else {
      this.#pendingClose.resolve();
    }
  }

  get closed() { return this.#pendingClose.promise; }

  // --- Properties ---

  get address() {
    if (this.#handle === null) return undefined;
    return this.#handle.getAddress();
  }

  get state() { return this.#state; }
  get stats() { return this.#stats; }
  get sessions() { return this.#sessions; }

  get onerror() { return this.#onerror; }
  set onerror(fn) {
    if (fn !== undefined && fn !== null) {
      validateFunction(fn, 'onerror');
      this.#onerror = fn;
    } else {
      this.#onerror = undefined;
    }
  }

  set busy(val) {
    this.#state.busy = !!val;
  }

  get busy() {
    return this.#state.busy;
  }

  // --- Internal callbacks ---

  #onEndpointClose() {
    this.#sessions.clear();
    this.#pendingClose.resolve();
    this.#handle = null;
  }

  #onEndpointError(message) {
    if (this.#onerror) {
      this.#onerror(new ERR_INVALID_STATE(message));
    }
  }

  #onSessionNew(handle) {
    const session = new DTLSSession(kPrivateConstructor, handle, this);
    this.#sessions.add(session);
    if (this.#onsession) {
      this.#onsession(session);
    }
  }

  async [SymbolAsyncDispose]() {
    await this.close();
  }
}

// ============================================================================
// Public API functions
// ============================================================================

function createContext(options = kEmptyObject) {
  validateObject(options, 'options');

  const isServer = options.isServer === true;
  const context = new DTLSContext_(isServer);

  // Certificate
  if (options.cert !== undefined) {
    let cert = options.cert;
    if (Buffer.isBuffer(cert)) cert = cert.toString();
    validateString(cert, 'options.cert');
    context.setCert(cert);
  }

  // Private key
  if (options.key !== undefined) {
    let key = options.key;
    if (Buffer.isBuffer(key)) key = key.toString();
    validateString(key, 'options.key');
    context.setKey(key);
  }

  // CA certificates: if custom CAs are provided, use only those.
  // Otherwise load system default CAs. This matches Node.js TLS behavior.
  if (options.ca !== undefined) {
    const cas = ArrayIsArray(options.ca) ? options.ca : [options.ca];
    for (let ca of cas) {
      if (Buffer.isBuffer(ca)) ca = ca.toString();
      validateString(ca, 'options.ca');
      context.addCACert(ca);
    }
  } else {
    context.loadDefaultCAs();
  }

  // Ciphers
  if (options.ciphers !== undefined) {
    validateString(options.ciphers, 'options.ciphers');
    context.setCiphers(options.ciphers);
  }

  // ECDH curve (default: 'auto' = OpenSSL default selection)
  const ecdhCurve = options.ecdhCurve || 'auto';
  validateString(ecdhCurve, 'options.ecdhCurve');
  context.setECDHCurve(ecdhCurve);

  // ALPN protocols
  if (options.alpn !== undefined) {
    let protocols = options.alpn;
    if (ArrayIsArray(protocols)) {
      // Convert string array to wire-format buffer.
      const bufs = [];
      for (const proto of protocols) {
        validateString(proto, 'options.alpn[]');
        const buf = Buffer.from(proto);
        bufs.push(Buffer.from([buf.length]), buf);
      }
      protocols = Buffer.concat(bufs);
    }
    if (!Buffer.isBuffer(protocols)) {
      throw new ERR_INVALID_ARG_TYPE(
        'options.alpn', ['string[]', 'Buffer'], protocols);
    }
    context.setALPN(protocols);
  }

  // SRTP profiles
  if (options.srtp !== undefined) {
    validateString(options.srtp, 'options.srtp');
    context.setSRTP(options.srtp);
  }

  // Verification mode
  if (options.rejectUnauthorized !== undefined) {
    const mode = options.rejectUnauthorized ?
      (SSL_VERIFY_PEER_VALUE | SSL_VERIFY_FAIL_IF_NO_PEER_CERT_VALUE) :
      SSL_VERIFY_NONE_VALUE;
    context.setVerifyMode(mode);
  } else if (options.requestCert) {
    context.setVerifyMode(
      SSL_VERIFY_PEER_VALUE | SSL_VERIFY_FAIL_IF_NO_PEER_CERT_VALUE);
  }

  return context;
}

/**
 * Start a DTLS server.
 * @param {Function} onsession Callback invoked for each new DTLS session.
 * @param {object} options Server configuration.
 * @param {string|Buffer} options.cert Server certificate (PEM).
 * @param {string|Buffer} options.key Server private key (PEM).
 * @param {string|Buffer|Array} [options.ca] CA certificates (PEM).
 * @param {string} [options.host] Bind address.
 * @param {number} options.port Bind port.
 * @param {number} [options.mtu] MTU for DTLS records.
 * @param {string[]} [options.alpn] ALPN protocol list.
 * @param {string} [options.srtp] SRTP profile string.
 * @param {boolean} [options.requestCert] Request client certificates.
 * @returns {DTLSEndpoint}
 */
function listen(onsession, options = kEmptyObject) {
  validateFunction(onsession, 'onsession');
  validateObject(options, 'options');

  if (options.cert === undefined) {
    throw new ERR_MISSING_ARGS('options.cert');
  }
  if (options.key === undefined) {
    throw new ERR_MISSING_ARGS('options.key');
  }
  if (options.port === undefined) {
    throw new ERR_MISSING_ARGS('options.port');
  }

  const host = options.host || '0.0.0.0';
  const port = options.port;

  validateString(host, 'options.host');
  validateInteger(port, 'options.port', 0, 65535);

  const context = createContext({
    ...options,
    isServer: true,
  });

  const endpoint = new DTLSEndpoint({
    mtu: options.mtu,
  });

  endpoint.bind(host, port);
  endpoint.listen(onsession, context);

  return endpoint;
}

/**
 * Connect to a DTLS server.
 * @param {string} host Remote host.
 * @param {number} port Remote port.
 * @param {object} [options] Client configuration.
 * @param {string|Buffer|Array} [options.ca] CA certificates (PEM).
 * @param {string|Buffer} [options.cert] Client certificate (PEM).
 * @param {string|Buffer} [options.key] Client private key (PEM).
 * @param {boolean} [options.rejectUnauthorized] When true (default), verify
 *   the server certificate against the trusted CAs and check its identity
 *   against servername (or host); aborts the handshake on failure.
 * @param {string} [options.servername] Server name for the SNI extension and
 *   the identity checked during certificate verification. Defaults to host;
 *   set to '' to disable SNI. Never sent for IP address literals.
 * @param {string} [options.bindHost] Local bind address.
 * @param {number} [options.bindPort] Local bind port (0 = ephemeral).
 * @param {number} [options.mtu] MTU for DTLS records.
 * @param {string[]} [options.alpn] ALPN protocol list.
 * @param {string} [options.srtp] SRTP profile string.
 * @returns {DTLSSession}
 */
function connect(host, port, options = kEmptyObject) {
  validateString(host, 'host');
  validateInteger(port, 'port', 0, 65535);
  validateObject(options, 'options');

  const bindHost = options.bindHost || '0.0.0.0';
  const bindPort = options.bindPort || 0;

  const context = createContext({
    ...options,
    isServer: false,
    rejectUnauthorized: options.rejectUnauthorized !== false,
  });

  const endpoint = new DTLSEndpoint({
    mtu: options.mtu,
  });

  endpoint.bind(bindHost, bindPort);

  // SNI and peer-identity verification are resolved inside
  // DTLSEndpoint.connect(), which defaults both to the host argument (matching
  // Node.js TLS). The identity is enforced whenever the context verifies, i.e.
  // unless rejectUnauthorized is false.
  const session = endpoint.connect(context, host, port, options.servername);
  // Mark that this session owns the endpoint so it gets closed
  // automatically when the session closes, allowing process exit.
  session.ownsEndpoint = true;
  return session;
}

module.exports = {
  connect,
  listen,
  createContext,
  DTLSEndpoint,
  DTLSSession,
};

/* c8 ignore stop */
