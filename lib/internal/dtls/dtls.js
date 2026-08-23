'use strict';

// TODO(@jasnell) Temporarily ignoring c8 coverage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  ArrayFrom,
  ArrayIsArray,
  ArrayPrototypeJoin,
  FunctionPrototypeBind,
  ObjectKeys,
  PromisePrototypeThen,
  PromiseWithResolvers,
  SafeSet,
  StringPrototypeSlice,
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
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_MISSING_ARGS,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const {
  validateBoolean,
  validateBuffer,
  validateFunction,
  validateObject,
  validateString,
  validateInteger,
  validateUint32,
} = require('internal/validators');

const {
  Buffer,
} = require('buffer');

const {
  isIP,
} = require('internal/net');

const {
  isArrayBufferView,
} = require('internal/util/types');

const {
  InternalX509Certificate,
} = require('internal/crypto/x509');

const {
  DTLSEndpointState,
  DTLSSessionState,
} = require('internal/dtls/state');

const {
  DTLSEndpointStats,
  DTLSSessionStats,
} = require('internal/dtls/stats');

const {
  kHandle,
  kIsServer,
  kBind,
  kDoConnect,
  kDoListen,
  kOwner,
  kPrivateConstructor,
  kFinishClose,
  kOwnsEndpoint,
  kRemoveSession,
  kSNIContexts,
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

// Default bounds on how many server sessions an endpoint will hold. Each one
// owns an SSL, two BIOs and a timer, so an unbounded table is a memory
// exhaustion vector for anyone willing to complete cookie exchanges. The
// per-host cap is what stops a single peer taking the whole table; it is set
// well above what a large NAT would plausibly need, and either can be raised,
// or set to 0 to disable, via dtls.listen().
// Upper bound on exported keying material. RFC 5705 sets no limit, but every
// real exporter wants tens of bytes -- DTLS-SRTP's is 60 -- and without a
// bound a caller can ask for a 4 GiB allocation. Rejecting that with a
// TypeError beats failing the allocation.
const kMaxKeyingMaterialLength = 65536;

// Matches node:tls, which derives the default from the process invocation so
// that two differently-launched servers do not share resumable sessions.
let sessionIdContextCache;
function defaultSessionIdContext() {
  if (sessionIdContextCache === undefined) {
    const { createHash } = require('crypto');
    sessionIdContextCache = StringPrototypeSlice(
      createHash('sha1')
        .update(ArrayPrototypeJoin(process.argv, ' '))
        .digest('hex'),
      0, 32);
  }
  return sessionIdContextCache;
}

// Walks a pre-encoded ALPN protocol list: a sequence of one length byte
// followed by that many bytes, exactly filling the buffer. RFC 7301 gives
// ProtocolName a 1..255 length, so a zero byte is malformed rather than an
// empty entry.
function validateALPNWireFormat(buf) {
  let offset = 0;
  while (offset < buf.length) {
    const len = buf[offset];
    if (len === 0) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.alpn', buf,
        `contains a zero-length protocol name at offset ${offset}`);
    }
    if (offset + 1 + len > buf.length) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.alpn', buf,
        `protocol name at offset ${offset} runs past the end of the buffer`);
    }
    offset += 1 + len;
  }
}

const kDefaultMaxSessions = 10000;
const kDefaultMaxSessionsPerHost = 1000;

// ============================================================================
// DTLSSession -- represents a single DTLS peer association
// ============================================================================

// Assigned from the static blocks below. These reach private fields that are
// deliberately not public API, for tests run with --expose-internals.
let getDTLSSessionState;
let getDTLSEndpointState;
let getDTLSEndpointSessions;

class DTLSSession {
  #handle;
  #endpoint;
  #state;
  #stats;
  #pendingOpen;
  #pendingClose;
  #openSettled = false;
  #peerX509Certificate;
  // The identity this connection verifies against; undefined on the server.
  // Used to bind a resumable session to the host it was authenticated for.
  #verifyHost;
  #onmessage;
  #onerror;
  #onhandshake;
  #onkeylog;
  #ownsEndpoint = false;

  constructor(privateSymbol, handle, endpoint, verifyHost) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }

    this.#handle = handle;
    this.#handle[kOwner] = this;
    this.#endpoint = endpoint;
    this.#verifyHost = verifyHost;
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
      // Tells C++ it is worth turning key material into JS strings. Without a
      // listener the secrets never leave OpenSSL.
      this.#state.hasKeylogListener = true;
    } else {
      this.#onkeylog = undefined;
      this.#state.hasKeylogListener = false;
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
    // Any view over bytes. A Uint8Array is the obvious thing to send and was
    // refused, while exportKeyingMaterial() next to it took one.
    if (!isArrayBufferView(data)) {
      throw new ERR_INVALID_ARG_TYPE(
        'data', ['string', 'Buffer', 'TypedArray', 'DataView'], data);
    }
    return this.#handle.send(data);
  }

  // --- Lifecycle ---

  // Settle `opened` when the session goes away before the handshake finished.
  // Without this, close() and destroy() settled only `closed`, and anything
  // awaiting `opened` waited forever. A handshake that already completed --
  // or already failed -- has settled it, and must not be overwritten.
  #settleOpenOnTeardown(error) {
    if (this.#openSettled) return;
    this.#openSettled = true;
    this.#pendingOpen.reject(
      error ??
        new ERR_INVALID_STATE(
          'Session was closed before the handshake completed'));
  }

  close() {
    if (this.#handle === null) return this.closed;
    const handle = this.#handle;
    this.#handle = null;
    handle.close();
    this.#settleOpenOnTeardown();
    return this.closed;
  }

  destroy(error) {
    if (this.#handle === null) return;
    const handle = this.#handle;
    this.#handle = null;
    handle.destroy();
    this.#stats[kFinishClose]();
    this.#settleOpenOnTeardown(error);
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

  get session() {
    // Server sessions are not resumable by this API: a server does not hold
    // an identity to bind the blob to, and it is the client that carries a
    // session between connections.
    if (this.#handle === null || this.#verifyHost === undefined) {
      return undefined;
    }
    const session = this.#handle.getSession();
    if (session === undefined) return undefined;
    return wrapSession(Buffer.from(session.buffer,
                                   session.byteOffset,
                                   session.byteLength),
                       this.#verifyHost);
  }

  get reused() {
    if (this.#handle === null) return false;
    return this.#handle.wasReused();
  }

  get peerX509Certificate() {
    // Cached deliberately, not just to hand back a stable object. The
    // underlying X509Certificate::GetPeerCert() is destructive on the client
    // side: with no peer certificate of its own to start from it lifts the
    // leaf out of the SSL's chain with sk_X509_delete(), so calling it a
    // second time yields a shorter chain and eventually nothing at all.
    // Calling it once and keeping the result is the only correct use.
    if (this.#peerX509Certificate !== undefined) {
      return this.#peerX509Certificate;
    }
    if (this.#handle === null) return undefined;

    const cert = this.#handle.getPeerX509Certificate();
    if (!cert) {
      // No peer certificate yet, or none at all. Not cached: before the
      // handshake completes there is nothing to take, and the call does not
      // disturb the chain when it finds none, so a later access can retry.
      return undefined;
    }

    // The binding hands back the raw handle; the public shape comes from
    // wrapping it, as node:tls does at the same point.
    this.#peerX509Certificate = new InternalX509Certificate(cert);
    return this.#peerX509Certificate;
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

  /**
   * The short X509 verification error code for the peer's certificate chain,
   * e.g. `'CERT_HAS_EXPIRED'` or `'UNABLE_TO_GET_ISSUER_CERT'`, or `undefined`
   * if the chain verified. A peer that presented no certificate at all reports
   * `'UNABLE_TO_GET_ISSUER_CERT'` rather than verifying.
   *
   * Only meaningful once the handshake has completed.
   * @type {string|undefined}
   */
  get authorizationError() {
    if (this.#handle === null) return undefined;
    return this.#handle.getVerifyError();
  }

  /**
   * Whether the peer presented a certificate chain that verified against the
   * configured CAs. Always false before the handshake completes.
   * @type {boolean}
   */
  get authorized() {
    if (this.#handle === null) return false;
    // Before the handshake the binding reports HANDSHAKE_INCOMPLETE rather
    // than undefined, so this reads false: undefined is "no fault found",
    // which would otherwise read as authorized.
    return this.#handle.getVerifyError() === undefined;
  }

  get stats() { return this.#stats; }

  /**
   * Whether the session has been destroyed.
   * @returns {boolean}
   */
  get destroyed() { return this.#state.destroyed; }

  static {
    getDTLSSessionState = function(session) {
      return session.#state;
    };
  }
  get endpoint() { return this.#endpoint; }

  exportKeyingMaterial(length, label, context) {
    if (this.#handle === null) {
      throw new ERR_INVALID_STATE('Session is destroyed');
    }
    validateUint32(length, 'length', true);
    if (length > kMaxKeyingMaterialLength) {
      throw new ERR_OUT_OF_RANGE(
        'length', `<= ${kMaxKeyingMaterialLength}`, length);
    }
    validateString(label, 'label');
    if (context !== undefined) {
      validateBuffer(context, 'context');
    }
    return this.#handle.exportKeyingMaterial(length, label, context);
  }

  // --- Internal callbacks (called from C++ via endpoint dispatch) ---

  [kSessionHandshake](protocol) {
    this.#openSettled = true;
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
    // The binding reports OpenSSL failures as a string, but hands back the
    // exception itself when one was thrown by a callback it invoked during
    // the handshake. Wrapping that would bury the user's own error, its type
    // and its stack inside a message.
    const error = typeof message === 'string' ?
      new ERR_INVALID_STATE(message) : message;
    if (this.#onerror) {
      this.#onerror(error);
    }
    this.#openSettled = true;
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
      endpoint[kRemoveSession](this);
      if (ownsEndpoint) {
        endpoint.close();
      }
    }
  }

  [kSessionClose]() {
    // A peer that closes mid-handshake settles `closed` here; `opened` has to
    // be settled too or it hangs.
    this.#settleOpenOnTeardown();
    // Stats stop tracking anything once the session is gone. Told so, they
    // keep the last values and report isConnected false; not told, they went
    // on claiming to be live and returning numbers that had stopped moving.
    this.#stats[kFinishClose]();
    this.#pendingClose.resolve();
    this.#handle = null;
    // Remove from the endpoint's JS-side session set.
    if (this.#endpoint) {
      this.#endpoint[kRemoveSession](this);
    }
    // If this session owns its endpoint (client-side connect()),
    // close the endpoint too so the process can exit.
    if (this.#ownsEndpoint && this.#endpoint) {
      this.#endpoint.close();
    }
  }

  // Mark that this session owns its endpoint (for client sessions
  // created by connect() where the endpoint is internal).
  // Not a public accessor. This says whether closing the session should take
  // the endpoint with it, which is true only for the endpoint connect()
  // creates for a single session. Setting it on a server session made closing
  // that one session tear down the listener and every other session on it.
  get [kOwnsEndpoint]() { return this.#ownsEndpoint; }
  set [kOwnsEndpoint](val) { this.#ownsEndpoint = val; }

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
    validateObject(options, 'options');
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
    if (options.handshakeTimeout !== undefined) {
      validateInteger(
        options.handshakeTimeout, 'options.handshakeTimeout', 0);
      this.#handle.setHandshakeTimeout(options.handshakeTimeout);
    }

    // Bounds on accepted server sessions. Zero disables the cap.
    const maxSessions = options.maxSessions ?? kDefaultMaxSessions;
    const maxSessionsPerHost =
      options.maxSessionsPerHost ?? kDefaultMaxSessionsPerHost;
    validateInteger(maxSessions, 'options.maxSessions', 0, 0xffffffff);
    validateInteger(
      maxSessionsPerHost, 'options.maxSessionsPerHost', 0, 0xffffffff);
    this.#handle.setSessionLimits(maxSessions, maxSessionsPerHost);

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
    });
  }

  // --- Server mode ---

  [kDoListen](callback, context) {
    validateFunction(callback, 'callback');
    this.#onsession = callback;
    this.#handle.listen(context);
    return this;
  }

  // --- Client mode ---

  [kDoConnect](context, host, port, servername, session) {
    // Resolve SNI and the expected peer identity here so that every caller of
    // the endpoint API -- not only the top-level dtls.connect() -- gets safe
    // defaults. The identity is always bound to the requested servername (or,
    // failing that, the host). OpenSSL only *enforces* it when the context is
    // in a verifying mode, so binding it is a no-op for non-verifying
    // (rejectUnauthorized: false) contexts.
    //
    // These are applied to the client SSL inside the binding, before the
    // handshake's ClientHello is emitted; they cannot be set afterwards.
    // The identity verified against is derived from servername below. A
    // non-string reached the binding's IsString() test, failed it, and became
    // a null verify host -- so SSL_set1_host() was never called and hostname
    // verification was silently skipped while chain verification still ran.
    // Validated here rather than in connect() because every path that
    // establishes a session comes through this method.
    if (servername !== undefined) {
      validateString(servername, 'options.servername');
    }

    let sni = servername !== undefined ? (servername || undefined) : host;
    if (sni !== undefined && isIP(sni) !== 0) {
      sni = undefined;  // SNI is never sent for IP literals (matching TLS).
    }
    const verifyHost = servername || host;
    const verifyIsIp = isIP(verifyHost) !== 0;

    // Checked against the identity this connection will actually verify,
    // before any of it reaches OpenSSL.
    const resume = session !== undefined ?
      unwrapSession(session, verifyHost) : undefined;

    const sessionHandle = this.#handle.connect(
      context, host, port, sni, verifyHost, verifyIsIp, resume);
    const newSession = new DTLSSession(
      kPrivateConstructor, sessionHandle, this, verifyHost);
    this.#sessions.add(newSession);
    return newSession;
  }

  // --- Bind ---

  [kBind](host, port) {
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

    // The binding destroys the sessions too, and emits nothing for them, so
    // anything awaiting one would wait for a session that no longer exists.
    // Torn down here instead, with the endpoint's error, before the endpoint
    // goes. Copied because destroying a session can remove it from the set.
    for (const session of ArrayFrom(this.#sessions)) {
      session.destroy(error);
    }
    this.#sessions.clear();

    handle.destroy();
    this.#stats[kFinishClose]();
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

  get stats() { return this.#stats; }

  /**
   * Whether the endpoint has been destroyed.
   * @returns {boolean}
   */
  get destroyed() { return this.#state.destroyed; }

  [kRemoveSession](session) {
    this.#sessions.delete(session);
  }

  static {
    getDTLSEndpointState = function(endpoint) {
      return endpoint.#state;
    };

    getDTLSEndpointSessions = function(endpoint) {
      return endpoint.#sessions;
    };
  }

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
    this.#stats[kFinishClose]();
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

  // Server-only options were each treated differently on a client context:
  // sni threw, sessionIdContext was ignored, ticketKeys was applied anyway,
  // and requestCert was validated and then ignored. Rejected uniformly, so
  // that naming one on a client is a mistake the caller hears about.
  if (!isServer) {
    for (const name of kServerOnlyContextOptions) {
      if (options[name] !== undefined) {
        throw new ERR_INVALID_ARG_VALUE(
          `options.${name}`, options[name],
          'is only meaningful for a server context');
      }
    }
  }

  const context = new DTLSContext_(isServer);

  // Pre-shared keys (RFC 4279).
  if (options.psk !== undefined) {
    applyPSK(context, options.psk, isServer, options.pskIdentityHint);
  } else if (options.pskIdentityHint !== undefined) {
    // The hint names which key a client should pick. Without one to name it
    // was dropped, and the handshake then failed for want of a PSK with no
    // mention of the option that had been set.
    throw new ERR_INVALID_ARG_VALUE(
      'options.pskIdentityHint', options.pskIdentityHint,
      'is only meaningful together with options.psk');
  }

  // Session ticket keys. Without them OpenSSL generates a random key per
  // context, so tickets stop working the moment the process restarts or a
  // second endpoint is involved -- each has its own key and rejects the
  // other's tickets, falling back to a full handshake.
  if (options.ticketKeys !== undefined) {
    validateBuffer(options.ticketKeys, 'options.ticketKeys');
    // The required length is OpenSSL's, so the binding checks it and reports
    // what it wanted rather than this repeating a number that is not ours.
    context.setTicketKeys(options.ticketKeys);
  }

  // Certificate
  if (options.cert !== undefined) {
    let cert = options.cert;
    if (Buffer.isBuffer(cert)) cert = cert.toString();
    validateString(cert, 'options.cert');
    context.setCert(cert);
  }

  // Private key. The passphrase is a string only, matching node:tls, even
  // though key and cert also accept a Buffer.
  const { passphrase } = options;
  if (passphrase !== undefined && passphrase !== null) {
    validateString(passphrase, 'options.passphrase');
  }

  if (options.key !== undefined) {
    let key = options.key;
    if (Buffer.isBuffer(key)) key = key.toString();
    validateString(key, 'options.key');
    context.setKey(key, passphrase);
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
  } else if (options.psk !== undefined) {
    // The default cipher list excludes PSK, so a psk option with no ciphers
    // would negotiate nothing and fail with a bare handshake_failure. Turn
    // the suites on rather than leave that trap, but only when the caller has
    // not stated a preference of their own.
    //
    // Forward-secret suites come first: plain PSK derives its keys from the
    // shared secret alone, so anyone who later learns that key can decrypt
    // recorded traffic. DEFAULT stays on the end so a context that also has a
    // certificate can still use it.
    context.setCiphers(isServer ? kPSKCiphersServer : kPSKCiphersClient);
  }

  // ECDH curve (default: 'auto' = OpenSSL default selection)
  const ecdhCurve = options.ecdhCurve || 'auto';
  validateString(ecdhCurve, 'options.ecdhCurve');
  context.setECDHCurve(ecdhCurve);

  // ALPN protocols
  if (options.alpn !== undefined) {
    let protocols = options.alpn;
    if (ArrayIsArray(protocols)) {
      // Convert string array to wire-format buffer. Each entry is a single
      // length byte followed by that many bytes, so a name outside 1..255
      // cannot be represented: 256 would truncate to a zero length byte and
      // desynchronise the whole list.
      const bufs = [];
      for (let i = 0; i < protocols.length; i++) {
        validateString(protocols[i], `options.alpn[${i}]`);
        const buf = Buffer.from(protocols[i]);
        if (buf.length < 1 || buf.length > 255) {
          throw new ERR_OUT_OF_RANGE(
            `options.alpn[${i}] byte length`, '>= 1 && <= 255', buf.length);
        }
        bufs.push(Buffer.from([buf.length]), buf);
      }
      protocols = Buffer.concat(bufs);
    } else if (Buffer.isBuffer(protocols)) {
      // A pre-encoded list. Walk it so a malformed one is rejected here
      // rather than as an opaque handshake failure later.
      validateALPNWireFormat(protocols);
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

  // Verification mode.
  //
  // A server only asks for a client certificate when requestCert says to;
  // rejectUnauthorized then decides whether a missing or unverifiable one is
  // fatal. A client always verifies the server, and rejectUnauthorized decides
  // whether a failure is fatal.
  //
  // The permissive verify callback is installed in exactly one case: a server
  // that asked for a certificate but disabled rejection. That is the only
  // combination where OpenSSL would otherwise abort a handshake the
  // application has said it wants to judge for itself. Elsewhere OpenSSL keeps
  // enforcing, so a rejected peer gets a proper alert rather than a silently
  // dropped session, and no state is held for a peer that is about to be
  // turned away.
  //
  // A client under SSL_VERIFY_NONE still has its chain verified -- OpenSSL
  // simply does not abort -- so session.authorized stays accurate there
  // without any callback.
  // Compared against false rather than coerced, so anything else has to be a
  // boolean: rejectUnauthorized: 0 read as "do not verify" and meant the
  // opposite, and isServer: 'yes' produced a client.
  if (options.rejectUnauthorized !== undefined) {
    validateBoolean(options.rejectUnauthorized, 'options.rejectUnauthorized');
  }
  const rejectUnauthorized = options.rejectUnauthorized !== false;
  if (options.requestCert !== undefined) {
    validateBoolean(options.requestCert, 'options.requestCert');
  }

  if (isServer) {
    // Scope cached sessions to this server, the way node:tls does. Without an
    // id context OpenSSL will resume a session against a server configured
    // differently from the one that issued it.
    let sessionIdContext = options.sessionIdContext;
    if (sessionIdContext === undefined) {
      sessionIdContext = defaultSessionIdContext();
    } else {
      validateString(sessionIdContext, 'options.sessionIdContext');
      // SSL_MAX_SID_CTX_LENGTH. Checked here so the caller is told the limit
      // rather than getting a bare "failed to set session id context".
      if (Buffer.byteLength(sessionIdContext) > 32) {
        throw new ERR_OUT_OF_RANGE(
          'options.sessionIdContext', '<= 32 bytes',
          `${Buffer.byteLength(sessionIdContext)} bytes`);
      }
    }
    context.setSessionIdContext(sessionIdContext);

    if (options.requestCert) {
      context.setVerifyMode(
        rejectUnauthorized ?
          (SSL_VERIFY_PEER_VALUE | SSL_VERIFY_FAIL_IF_NO_PEER_CERT_VALUE) :
          SSL_VERIFY_PEER_VALUE,
        !rejectUnauthorized);
    } else {
      // Nothing is requested, so there is nothing to reject.
      context.setVerifyMode(SSL_VERIFY_NONE_VALUE, false);
    }
  } else {
    context.setVerifyMode(
      rejectUnauthorized ? SSL_VERIFY_PEER_VALUE : SSL_VERIFY_NONE_VALUE,
      false);
  }

  // Server Name Indication. Part of the context because it is a set of
  // identities to serve, and because applying it to a context built
  // elsewhere would reconfigure that context for everything else using it.
  if (options.sni !== undefined) {
    applySNIContexts(context, options.sni);
  }

  return context;
}

// Cipher list used when psk is given and ciphers is not.
//
// DEFAULT has to come first: OpenSSL only honours it as the opening
// cipherstring, and trailing it contributes nothing -- a list ending in
// DEFAULT silently has no certificate suites at all, so a server holding both
// a certificate and a PSK could only ever use the PSK.
//
// kECDHEPSK and kDHEPSK are the forward-secret PSK key exchanges and kPSK is
// plain PSK. OpenSSL orders what follows by cipher strength, so a
// forward-secret suite comes ahead of a plain one of equal strength, though
// not ahead of a stronger cipher without it. kRSAPSK is dropped: it needs a
// certificate and gains no forward secrecy for the extra round trip.
// A server may serve certificate clients and PSK clients on the same port,
// so it keeps both. A client is one or the other, and one that configured a
// pre-shared key and no CA wants the key: leaving the certificate suites on
// lets a server pick one, and the handshake then fails verifying a
// certificate the caller never meant to rely on.
const kPSKCiphersServer = 'DEFAULT:kECDHEPSK:kDHEPSK:kPSK:!kRSAPSK';
const kPSKCiphersClient = 'kECDHEPSK:kDHEPSK:kPSK:!kRSAPSK';

/**
 * Configure pre-shared keys on a context.
 * @param {object} context Native context.
 * @param {object|Function} psk Identity map, {identity, key}, or a callback.
 * @param {boolean} isServer Which side is being configured.
 * @param {string} [hint] Identity hint to advertise (server only).
 */
function applyPSK(context, psk, isServer, hint) {
  if (hint !== undefined) {
    if (!isServer) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.pskIdentityHint', hint, 'is only meaningful for a server');
    }
    validateString(hint, 'options.pskIdentityHint');
  }

  let entries;
  let identity;
  let key;
  let callback;

  if (typeof psk === 'function') {
    callback = psk;
  } else {
    validateObject(psk, 'options.psk');

    if (isServer) {
      // A map of the identities this server will accept.
      entries = [];
      for (const name of ObjectKeys(psk)) {
        const value = psk[name];
        validateBuffer(value, `options.psk['${name}']`);
        if (value.byteLength === 0) {
          throw new ERR_INVALID_ARG_VALUE(
            `options.psk['${name}']`, value, 'must not be empty');
        }
        entries.push(name, value);
      }
      if (entries.length === 0) {
        throw new ERR_INVALID_ARG_VALUE(
          'options.psk', psk, 'must have at least one entry');
      }
    } else {
      // The single identity this client presents.
      ({ identity, key } = psk);
      validateString(identity, 'options.psk.identity');
      validateBuffer(key, 'options.psk.key');
      if (key.byteLength === 0) {
        throw new ERR_INVALID_ARG_VALUE(
          'options.psk.key', key, 'must not be empty');
      }
    }
  }

  context.setPSK(entries, hint, identity, key, callback);
}

// Resumable sessions are bound to the identity they were authenticated for.
//
// A resumed handshake does not re-send or re-verify the peer's certificate:
// it inherits the authenticated identity of the original session. Replaying a
// session against a different host therefore skips verification while looking
// like it succeeded. node:tls was fixed for this in CVE-2026-48934; the same
// hazard exists here the moment a session blob can be carried between
// connections, so the blob is prefixed with the identity it belongs to and
// refused anywhere else.
//
// The identity used is exactly the verifyHost handed to the binding for
// SSL_set1_host(), so the value that gets bound and the value that gets
// verified cannot drift apart.
const kSessionPrefix = Buffer.from('\0nodejs:dtls:session:1\0');

function wrapSession(session, verifyHost) {
  const identity = Buffer.from(verifyHost, 'utf8');
  const length = Buffer.allocUnsafe(2);
  length.writeUInt16BE(identity.length, 0);
  return Buffer.concat([kSessionPrefix, length, identity, session]);
}

function unwrapSession(session, verifyHost) {
  // Not being a Buffer is a type error. It was folded in with the checks
  // below and reported as ERR_INVALID_ARG_VALUE, so passing a string got the
  // code that means "the right type, the wrong contents".
  if (!Buffer.isBuffer(session)) {
    throw new ERR_INVALID_ARG_TYPE('options.session', 'Buffer', session);
  }
  if (session.length < kSessionPrefix.length + 2 ||
      Buffer.compare(session.subarray(0, kSessionPrefix.length),
                     kSessionPrefix) !== 0) {
    // Not one of ours. It may be a session blob from somewhere else, but
    // nothing here can say what identity it was authenticated for, so it
    // cannot safely be resumed.
    throw new ERR_INVALID_ARG_VALUE(
      'options.session', session,
      'is not a session produced by session.session');
  }

  const start = kSessionPrefix.length;
  const identityLength = session.readUInt16BE(start);
  const identityStart = start + 2;
  const identityEnd = identityStart + identityLength;
  if (session.length < identityEnd) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.session', session, 'is truncated');
  }

  const identity = session.toString('utf8', identityStart, identityEnd);
  if (identity !== verifyHost) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.session', session,
      `was authenticated for '${identity}' and cannot be reused for ` +
      `'${verifyHost}'`);
  }

  return session.subarray(identityEnd);
}

// The options createContext() consumes. Anything else -- host, port, mtu,
// servername and so on -- belongs to the endpoint or the individual
// connection, not the context, and stays legal alongside a secureContext.
// Options that only a server context can act on. A client naming one has
// misunderstood the option, so it is refused rather than quietly dropped.
const kServerOnlyContextOptions = [
  'pskIdentityHint', 'requestCert', 'sessionIdContext', 'sni', 'ticketKeys',
];

const kContextOptions = [
  'alpn', 'ca', 'cert', 'ciphers', 'ecdhCurve', 'key', 'passphrase',
  'psk', 'pskIdentityHint', 'rejectUnauthorized', 'requestCert',
  'sessionIdContext', 'sni', 'srtp', 'ticketKeys',
];

/**
 * An opaque, reusable bundle of credentials and TLS settings. Build it with
 * createSecureContext() and hand it to listen() or connect().
 */
class DTLSSecureContext {
  constructor(privateSymbol, handle, isServer) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    this[kHandle] = handle;
    this[kIsServer] = isServer;
  }

  get isServer() {
    return this[kIsServer];
  }
}

/**
 * Create a reusable secure context.
 * @param {object} [options] Credentials and TLS settings.
 * @param {boolean} [options.isServer] Build a server context. A context can
 *   only be used by the side it was built for.
 * @returns {DTLSSecureContext}
 */
function createSecureContext(options = kEmptyObject) {
  validateObject(options, 'options');
  if (options.isServer !== undefined) {
    validateBoolean(options.isServer, 'options.isServer');
  }
  const isServer = options.isServer === true;
  return new DTLSSecureContext(
    kPrivateConstructor, createContext(options), isServer);
}

/**
 * Build the SNI map and install it on a server context.
 *
 * Values may be a DTLSSecureContext or a plain options bag; the bag is just
 * shorthand for createSecureContext({ ...bag, isServer: true }).
 * @param {object} context The endpoint's own native context.
 * @param {object} sni Host name to context or options.
 */
/**
 * Normalize what an sni map entry or callback produced into a native
 * context handle.
 * @param {object|DTLSSecureContext} value The configured or returned value.
 * @param {string} name Argument name for errors.
 * @returns {object} Native context handle.
 */
function resolveSNIValue(value, name) {
  if (value instanceof DTLSSecureContext) {
    if (!value.isServer) {
      throw new ERR_INVALID_ARG_VALUE(
        name, value,
        'was created for a client; SNI contexts need isServer: true');
    }
    return value[kHandle];
  }
  validateObject(value, name);
  return createSecureContext({ ...value, isServer: true })[kHandle];
}

function applySNIContexts(context, sni) {
  if (typeof sni === 'function') {
    // The binding calls this during the handshake and wants a context back,
    // so the conversion an options bag needs is done here rather than there.
    // Building one costs a certificate parse on every handshake, which is
    // why returning a prepared DTLSSecureContext is worth doing.
    const select = (servername) => {
      const value = sni(servername);
      if (value === undefined || value === null) return undefined;
      return resolveSNIValue(value, 'the value returned by options.sni');
    };
    context.setSNIContexts([], select);
    return;
  }

  validateObject(sni, 'options.sni');

  const hostnames = ObjectKeys(sni);
  if (hostnames.length === 0) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.sni', sni, 'must have at least one entry');
  }

  // Flattened to [host, ctx, host, ctx, ...]: a plain array crosses into C++
  // without the binding having to walk a JS object.
  const flat = [];
  for (const hostname of hostnames) {
    flat.push(hostname,
              resolveSNIValue(sni[hostname], `options.sni['${hostname}']`));
  }

  // The binding holds these weakly. A context is allowed to appear in its own
  // SNI map, or in a cycle with another, and a reference count cannot free
  // either -- but a property on the owning wrapper is an edge the garbage
  // collector can trace, so the whole group goes when nothing else holds it.
  context[kSNIContexts] = flat;
  context.setSNIContexts(flat, undefined);
}

/**
 * Resolve the context for listen()/connect(): either the caller's reusable
 * one or a fresh one built from the inline options.
 * @param {object} options The full option bag.
 * @param {boolean} isServer Which side is asking.
 * @param {object} extra Fields to force when building inline.
 * @returns {object} The native context handle.
 */
function resolveSecureContext(options, isServer, extra) {
  const { secureContext } = options;
  if (secureContext === undefined) {
    return createContext({ ...options, ...extra, isServer });
  }

  if (!(secureContext instanceof DTLSSecureContext)) {
    throw new ERR_INVALID_ARG_TYPE(
      'options.secureContext', 'DTLSSecureContext', secureContext);
  }

  // A context built for one side cannot serve the other: isServer selects
  // the OpenSSL method when the context is created, long before this point.
  if (secureContext.isServer !== isServer) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.secureContext', secureContext,
      isServer ?
        'was created for a client; listen() needs isServer: true' :
        'was created for a server; connect() needs isServer: false');
  }

  // Credentials baked into the context cannot be overridden per call, so
  // say that rather than silently ignoring them.
  for (const name of kContextOptions) {
    if (options[name] !== undefined) {
      throw new ERR_INVALID_ARG_VALUE(
        `options.${name}`, options[name],
        'cannot be combined with options.secureContext; ' +
        'set it on the context instead');
    }
  }

  return secureContext[kHandle];
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
 * @param {number} [options.maxSessions] Maximum concurrent sessions.
 * @param {number} [options.maxSessionsPerHost] Maximum concurrent sessions
 *   from any single source address.
 * @returns {DTLSEndpoint}
 */
function listen(onsession, options = kEmptyObject) {
  validateFunction(onsession, 'onsession');
  validateObject(options, 'options');

  // A server needs a certificate and key, but they may already be in a
  // secureContext, in which case supplying them here is an error anyway.
  if (options.secureContext === undefined && options.psk === undefined) {
    // A PSK-only server authenticates with the shared key and has no
    // certificate to present.
    if (options.cert === undefined) {
      throw new ERR_MISSING_ARGS('options.cert');
    }
    if (options.key === undefined) {
      throw new ERR_MISSING_ARGS('options.key');
    }
  }
  if (options.port === undefined) {
    throw new ERR_MISSING_ARGS('options.port');
  }

  const host = options.host || '0.0.0.0';
  const port = options.port;

  validateString(host, 'options.host');
  validateInteger(port, 'options.port', 0, 65535);

  const context = resolveSecureContext(options, true, kEmptyObject);

  const endpoint = new DTLSEndpoint({
    mtu: options.mtu,
    handshakeTimeout: options.handshakeTimeout,
    maxSessions: options.maxSessions,
    maxSessionsPerHost: options.maxSessionsPerHost,
  });

  endpoint[kBind](host, port);
  endpoint[kDoListen](onsession, context);

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

  // The local socket has to be in the same family as the peer: binding the
  // IPv4 wildcard and then sending to an IPv6 address cannot work. Default to
  // the wildcard matching the remote literal. isIP() only parses, so this
  // stays synchronous -- a hostname returns 0 and keeps the IPv4 default,
  // which is what happens today for anything DNS would have resolved.
  if (options.bindHost !== undefined) {
    validateString(options.bindHost, 'options.bindHost');
  }
  if (options.bindPort !== undefined) {
    validateInteger(options.bindPort, 'options.bindPort', 0, 65535);
  }
  const bindHost =
    options.bindHost || (isIP(host) === 6 ? '::' : '0.0.0.0');
  const bindPort = options.bindPort || 0;

  if (options.rejectUnauthorized !== undefined) {
    validateBoolean(options.rejectUnauthorized, 'options.rejectUnauthorized');
  }
  const context = resolveSecureContext(options, false, {
    rejectUnauthorized: options.rejectUnauthorized !== false,
  });

  const endpoint = new DTLSEndpoint({
    mtu: options.mtu,
    handshakeTimeout: options.handshakeTimeout,
  });

  endpoint[kBind](bindHost, bindPort);

  // SNI and peer-identity verification are resolved inside
  // DTLSEndpoint.connect(), which defaults both to the host argument (matching
  // Node.js TLS). The identity is enforced whenever the context verifies, i.e.
  // unless rejectUnauthorized is false.
  const session = endpoint[kDoConnect](
    context, host, port, options.servername, options.session);
  // Mark that this session owns the endpoint so it gets closed
  // automatically when the session closes, allowing process exit.
  session[kOwnsEndpoint] = true;
  return session;
}

module.exports = {
  connect,
  listen,
  createContext,
  createSecureContext,
  DTLSEndpoint,
  DTLSSecureContext,
  DTLSSession,
  // Not public API. Exposed for tests run with --expose-internals.
  getDTLSSessionState,
  getDTLSEndpointState,
  getDTLSEndpointSessions,
};

/* c8 ignore stop */
