'use strict';

const {
  ObjectDefineProperties,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  ReflectConstruct,
  SafeSet,
  Symbol,
} = primordials;

const {
  createEndpoint: _createEndpoint,
} = internalBinding('quic');

// If the _createEndpoint is undefined, the Node.js binary
// was built without QUIC support, in which case we
// don't want to export anything here.
if (_createEndpoint === undefined)
  return;

const {
  InternalSocketAddress,
  SocketAddress,
  kHandle: kSocketAddressHandle,
} = require('internal/socketaddress');

const kInit = Symbol('kInit');

const { owner_symbol } = internalBinding('symbols');

const {
  makeTransferable,
  kClone,
  kDeserialize,
} = require('internal/worker/js_transferable');

const {
  defineEventHandler,
  NodeEventTarget,
} = require('internal/event_target');

const {
  EndpointConfig,
  SessionConfig,
  kHandle,
  kSecureContext,
  validateResumeOptions,
} = require('internal/quic/config');

const {
  createEndpointStats,
  kDetach: kDetachStats,
  kStats,
} = require('internal/quic/stats');

const {
  createSession,
  sessionDestroy,
} = require('internal/quic/session');

const {
  createDeferredPromise,
  customInspectSymbol: kInspect,
} = require('internal/util');

const {
  createSessionEvent,
  setPromiseHandled,
  kAddSession,
  kRemoveSession,
  kState,
  kType,
} = require('internal/quic/common');

const {
  kEnumerableProperty,
} = require('internal/webstreams/util');

const {
  inspect,
} = require('util');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const { validateObject } = require('internal/validators');

/**
 * @typedef {import('../socketaddress').SocketAddress} SocketAddress
 * @typedef {import('../socketaddress').SocketAddressOrOptions
 * } SocketAddressOrOptions
 * @typedef {import ('./session').Session} Session
 * @typedef {import('./config').SessionConfigOrOptions} SessionConfigOrOptions
 * @typedef {import('./config').EndpointConfigOrOptions} EndpointConfigOrOptions
 */

class Endpoint extends NodeEventTarget {
  /**
   * @param {EndpointConfigOrOptions} [options]
   */
  constructor(options = new EndpointConfig()) {
    if (!EndpointConfig.isEndpointConfig(options)) {
      if (options === null || typeof options !== 'object') {
        throw new ERR_INVALID_ARG_TYPE('options', [
          'EndpointConfig',
          'Object',
        ], options);
      }
      options = new EndpointConfig(options);
    }

    super();
    this[kInit](_createEndpoint(options[kHandle]));
    const ret = makeTransferable(this);
    this[kHandle][owner_symbol] = ret;
    /* eslint-disable-next-line no-constructor-return */
    return ret;
  }

  [kInit](handle) {
    this[kType] = 'Endpoint';
    this[kState] = {
      closed: createDeferredPromise(),
      closeRequested: false,
      listening: false,
      sessions: new SafeSet(),
    };
    setPromiseHandled(this[kState].closed.promise);

    this[kHandle] = handle;
    this[kStats] = createEndpointStats(this[kHandle].stats);
  }

  [kAddSession](sessionHandle) {
    const session = createSession(sessionHandle, this);
    this[kState].sessions.add(session);
    PromisePrototypeThen(
      PromiseResolve(),
      () => this.dispatchEvent(createSessionEvent(session)),
      (error) => sessionDestroy(session, error));
  }

  [kRemoveSession](session) {
    this[kState].sessions.delete(session);
  }

  /**
   * Instruct the Endpoint to being listening for new inbound
   * initial QUIC packets. If the Endpoint is not yet bound
   * to the local UDP port, it will be bound automatically.
   * @param {SessionConfigOrOptions} [options]
   * @returns {Endpoint}
   */
  listen(options = new SessionConfig('server')) {
    if (!isEndpoint(this))
      throw new ERR_INVALID_THIS('Endpoint');
    if (isEndpointDestroyed(this))
      throw new ERR_INVALID_STATE('Endpoint is already destroyed');
    if (this[kState].closeRequested)
      throw new ERR_INVALID_STATE('Endpoint is closing');
    if (this[kState].listening)
      throw new ERR_INVALID_STATE('Endpoint is already listening');

    if (!SessionConfig.isSessionConfig(options)) {
      if (options === null || typeof options !== 'object') {
        throw new ERR_INVALID_ARG_TYPE('options', [
          'SessionConfig',
          'Object',
        ], options);
      }
      options = new SessionConfig('server', options);
    }
    if (options.side !== 'server') {
      throw new ERR_INVALID_ARG_VALUE(
        'options',
        options,
        'must be a server SessionConfig');
    }

    this[kHandle].listen(options[kHandle], options[kSecureContext]);
    this[kState].listening = true;
    return this;
  }

  /**
   * @param {SocketAddressOrOptions} address
   * @param {SessionConfigOrOptions} [options]
   * @param {Object} [resume]
   * @param {ArrayBuffer | TypedArray | DataView} resume.sessionTicket
   * @param {ArrayBuffer | TypedArray | DataView} resume.transportParams
   * @returns {Session}
   */
  connect(address, options = new SessionConfig('client'), resume = {}) {
    if (!isEndpoint(this))
      throw new ERR_INVALID_THIS('Endpoint');
    if (isEndpointDestroyed(this))
      throw new ERR_INVALID_STATE('Endpoint is already destroyed');
    if (this[kState].closeRequested)
      throw new ERR_INVALID_STATE('Endpoint is closing');

    if (!SocketAddress.isSocketAddress(address)) {
      if (typeof address !== 'object' || address == null) {
        throw new ERR_INVALID_ARG_TYPE('address', [
          'SocketAddress',
          'Object',
        ], address);
      }
      address = new SocketAddress(address);
    }

    if (!SessionConfig.isSessionConfig(options)) {
      if (options === null || typeof options !== 'object') {
        throw new ERR_INVALID_ARG_TYPE('options', [
          'SessionConfig',
          'Object',
        ], options);
      }
      options = new SessionConfig('client', options);
    }

    if (options.side !== 'client') {
      throw new ERR_INVALID_ARG_VALUE(
        'options',
        options,
        'must be a client SessionConfig');
    }

    validateObject(resume, 'resume');
    const {
      sessionTicket,
      transportParams,
    } = resume;
    validateResumeOptions(sessionTicket, transportParams);

    const session = createSession(
      this[kHandle].createClientSession(
        address[kSocketAddressHandle],
        options[kHandle],
        options[kSecureContext],
        sessionTicket,
        transportParams),
      this,
      options.alpn,
      `${options.hostname || address.address}:${address.port}`);

    this[kState].sessions.add(session);

    return session;
  }

  /**
   * @readonly
   * @type {Promise<void>}
   */
  get closed() {
    if (!isEndpoint(this))
      return PromiseReject(new ERR_INVALID_THIS('Endpoint'));
    return this[kState].closed.promise;
  }

  /**
   * Begins a graceful close of the Endpoint.
   * * If the Endpoint is listening, new inbound Initial packets will be
   *   rejected.
   * * Attempts to create new outbound Sessions using connect() will be
   *   immediately rejected.
   * * Existing Sessions will be allowed to finish naturally, after which
   *   the Endpoint will be immediately destroyed.
   * * The Promise returned will be resolved when the Endpoint is destroyed,
   *   or rejected if a fatal errors occurs.
   *  @returns {Promise<void>}
   */
  close() {
    if (!isEndpoint(this))
      return PromiseReject(new ERR_INVALID_THIS('Endpoint'));
    if (isEndpointDestroyed(this)) {
      return PromiseReject(
        new ERR_INVALID_STATE('Endpoint is already destroyed'));
    }

    if (!this[kState].closeRequested) {
      this[kState].closeRequested = true;
      this[kHandle].waitForPendingCallbacks();
    }
    return this[kState].closed.promise;
  }

  /**
   * Immediately destroys the Endpoint.
   * * Any existing Sessions will be immediately, and abruptly terminated.
   * * The reference to the underlying EndpointWrap handle will be released
   *   allowing it to be garbage collected as soon as possible.
   * * The stats will be detached from the underlying EndpointWrap
   * @param {Error} [error]
   * @returns {void}
   */
  destroy(error) {
    if (!isEndpoint(this))
      throw new ERR_INVALID_THIS('Endpoint');
    endpointDestroy(this, error);
  }

  /**
   * @readonly
   * @type {SocketAddress | void}
   */
  get address() {
    if (!isEndpoint(this))
      throw new ERR_INVALID_THIS('Endpoint');
    return endpointAddress(this);
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get closing() {
    if (!isEndpoint(this))
      throw new ERR_INVALID_THIS('Endpoint');
    return this[kState].closeRequested;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get listening() {
    if (!isEndpoint(this))
      throw new ERR_INVALID_THIS('Endpoint');
    return this[kState].listening;
  }

  /**
   * @readonly
   * @type {import('./stats').EndpointStats}
   */
  get stats() {
    if (!isEndpoint(this))
      throw new ERR_INVALID_THIS('Endpoint');
    return this[kStats];
  }

  [kInspect](depth, options) {
    if (!isEndpoint(this))
      throw new ERR_INVALID_THIS('Endpoint');

    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `${this[kType]} ${inspect({
      address: endpointAddress(this),
      closed: this[kState].closed.promise,
      closing: this[kState].closeRequested,
      listening: this[kState].listening,
      stats: this[kStats],
    }, opts)}`;
  }

  [kClone]() {
    if (!isEndpoint(this))
      throw new ERR_INVALID_THIS('Endpoint');

    const handle = this[kHandle];
    return {
      data: { handle },
      deserializeInfo: 'internal/quic/endpoint:createEndpoint',
    };
  }

  [kDeserialize]({ handle }) {
    this[kInit](handle);
  }
}

ObjectDefineProperties(Endpoint.prototype, {
  address: kEnumerableProperty,
  close: kEnumerableProperty,
  closed: kEnumerableProperty,
  closing: kEnumerableProperty,
  connect: kEnumerableProperty,
  destroy: kEnumerableProperty,
  listen: kEnumerableProperty,
  listening: kEnumerableProperty,
  stats: kEnumerableProperty,
});

function isEndpoint(value) {
  return typeof value?.[kState] === 'object' && value?.[kType] === 'Endpoint';
}

function createEndpoint() {
  const ret = makeTransferable(
    ReflectConstruct(
      class extends NodeEventTarget {},
      [],
      Endpoint));
  ret[kHandle][owner_symbol] = ret;
  return ret;
}

defineEventHandler(Endpoint.prototype, 'session');

function isEndpointDestroyed(endpoint) {
  return endpoint[kHandle] === undefined;
}

function endpointAddress(endpoint) {
  const state = endpoint[kState];
  if (!isEndpointDestroyed(endpoint) && state.address === undefined) {
    const handle = endpoint[kHandle]?.address();
    if (handle !== undefined)
      state.address = new InternalSocketAddress(handle);
  }
  return state.address;
}

function endpointDestroy(endpoint, error) {
  if (isEndpointDestroyed(endpoint))
    return;
  const state = endpoint[kState];
  state.destroyed = true;
  state.listening = false;
  state.address = undefined;

  for (const session of state.sessions)
    sessionDestroy(session, error);

  state.sessions = new SafeSet();

  endpoint[kStats][kDetachStats]();
  endpoint[kHandle] = undefined;

  if (error)
    state.closed.reject(error);
  else {
    state.closed.resolve();
  }
  state.closeRequested = false;
}

module.exports = {
  Endpoint,
  createEndpoint,
  isEndpoint,
};
