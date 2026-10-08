'use strict'

const {
  PoolBase,
  kClients,
  kNeedDrain,
  kAddClient,
  kDrainQueue,
  kOnClientBusy,
  kOnClientDrain,
  kGetDispatcher,
  kHasDispatcher,
  kRemoveClient
} = require('./pool-base')
const Client = require('./client')
const {
  InvalidArgumentError
} = require('../core/errors')
const util = require('../core/util')
const { kConnecting, kHTTPContext, kUrl } = require('../core/symbols')
const buildConnector = require('../core/connect')

const kOptions = Symbol('options')
const kConnections = Symbol('connections')
const kFactory = Symbol('factory')
const kProtocol = Symbol('protocol')
const kProtocolProbe = Symbol('protocol probe')

function defaultFactory (origin, opts) {
  return new Client(origin, opts)
}

function shouldCreateProtocolProbe (pool, dispatcher) {
  return dispatcher instanceof Client &&
    pool[kProtocol] !== 'h1' &&
    (pool[kOptions].useH2c === true || (pool[kUrl].protocol === 'https:' && pool[kOptions].allowH2 !== false))
}

function createClient (pool) {
  const dispatcher = pool[kFactory](pool[kUrl], pool[kOptions])

  // HTTPS does not reveal whether the peer selected h1 or h2 until ALPN
  // completes. While h2 is still possible, let one Client probe the protocol
  // and keep later requests in the Pool queue instead of opening one TLS
  // connection per request. A confirmed h1 connection disables this gate and
  // restores the usual Pool fan-out.
  if (shouldCreateProtocolProbe(pool, dispatcher)) {
    pool[kProtocolProbe] = dispatcher
  }

  pool[kAddClient](dispatcher)
  return dispatcher
}

class Pool extends PoolBase {
  constructor (origin, {
    connections,
    factory = defaultFactory,
    connect,
    connectTimeout,
    tls,
    maxCachedSessions,
    socketPath,
    autoSelectFamily,
    autoSelectFamilyAttemptTimeout,
    allowH2,
    useH2c,
    clientTtl,
    ...options
  } = {}) {
    if (connections != null && (!Number.isFinite(connections) || connections < 0)) {
      throw new InvalidArgumentError('invalid connections')
    }

    if (typeof factory !== 'function') {
      throw new InvalidArgumentError('factory must be a function.')
    }

    if (connect != null && typeof connect !== 'function' && typeof connect !== 'object') {
      throw new InvalidArgumentError('connect must be a function or an object')
    }

    if (typeof connect !== 'function') {
      connect = buildConnector({
        ...tls,
        maxCachedSessions,
        allowH2,
        useH2c,
        socketPath,
        timeout: connectTimeout,
        ...(typeof autoSelectFamily === 'boolean' ? { autoSelectFamily, autoSelectFamilyAttemptTimeout } : undefined),
        ...connect
      })
    }

    super(options)

    this[kConnections] = connections || null
    this[kUrl] = util.parseOrigin(origin)
    this[kOptions] = { ...util.deepClone(options), connect, allowH2, useH2c, clientTtl, socketPath }
    this[kFactory] = factory
    this[kProtocol] = null
    this[kProtocolProbe] = null

    this.on('connect', (origin, targets) => {
      if (clientTtl != null && clientTtl > 0) {
        for (const target of targets) {
          Object.assign(target, { ttl: Date.now() })
        }
      }

      const client = targets[targets.length - 1]
      if (client instanceof Client) {
        this[kProtocol] = client[kHTTPContext]?.version
      }

      if (client === this[kProtocolProbe]) {
        // An h2 Client's drain event releases the requests accumulated during
        // negotiation onto that Client. If ALPN selected h1, release the probe
        // immediately and restore normal Pool fan-out instead.
        if (this[kProtocol] !== 'h2') {
          this[kProtocolProbe] = null
          this[kDrainQueue](origin, targets.slice(1))
        }
      }
    })

    this.on('disconnect', (origin, targets) => {
      if (targets.includes(this[kProtocolProbe])) {
        this[kProtocolProbe] = null
        this[kDrainQueue](origin, targets.slice(1))
      }
    })

    this.on('connectionError', (origin, targets) => {
      let resumeQueued = false

      // If a connection error occurs, we remove the client from the pool,
      // and emit a connectionError event. They will not be re-used.
      // Fixes https://github.com/nodejs/undici/issues/3895
      for (const target of targets) {
        if (target === this[kProtocolProbe]) {
          this[kProtocolProbe] = null
          resumeQueued = true
        }

        // Do not use kRemoveClient here, as it will close the client,
        // but the client cannot be closed in this state.
        const idx = this[kClients].indexOf(target)
        if (idx !== -1) {
          this[kClients].splice(idx, 1)
        }
      }

      if (resumeQueued) {
        this[kDrainQueue](origin, targets.slice(1))
      }
    })
  }

  [kOnClientBusy] (client) {
    if (
      this[kProtocolProbe] === null &&
      client[kConnecting] &&
      shouldCreateProtocolProbe(this, client)
    ) {
      this[kProtocolProbe] = client
    }
  }

  [kOnClientDrain] (client) {
    if (client === this[kProtocolProbe]) {
      this[kProtocolProbe] = null
    }
  }

  [kGetDispatcher] () {
    const clientTtlOption = this[kOptions].clientTtl
    for (let i = 0; i < this[kClients].length; i++) {
      const client = this[kClients][i]

      // check ttl of client and if it's stale, remove it from the pool
      if (clientTtlOption != null && clientTtlOption > 0 && client.ttl && ((Date.now() - client.ttl) > clientTtlOption)) {
        this[kRemoveClient](client)
        i--
      } else if (!client[kNeedDrain]) {
        return client
      }
    }

    if (this[kProtocolProbe] !== null) {
      return
    }

    if (!this[kConnections] || this[kClients].length < this[kConnections]) {
      return createClient(this)
    }
  }

  [kHasDispatcher] () {
    const clientTtlOption = this[kOptions].clientTtl
    for (let i = 0; i < this[kClients].length; i++) {
      const client = this[kClients][i]

      if (clientTtlOption != null && clientTtlOption > 0 && client.ttl && ((Date.now() - client.ttl) > clientTtlOption)) {
        this[kRemoveClient](client)
        i--
      } else if (!client[kNeedDrain]) {
        return true
      }
    }

    if (this[kProtocolProbe] !== null) {
      return false
    }

    if (!this[kConnections] || this[kClients].length < this[kConnections]) {
      createClient(this)
      return true
    }

    return false
  }
}

module.exports = Pool
