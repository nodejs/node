'use strict'

const {
  ClientClosedError,
  InvalidArgumentError,
  ClientDestroyedError
} = require('./core/errors')
const { kClients, kRunning } = require('./core/symbols')
const Dispatcher = require('./dispatcher')
const Pool = require('./pool')
const Client = require('./client')
const util = require('./core/util')
const RedirectHandler = require('./handler/redirect')
const { WeakRef, FinalizationRegistry } = require('./compat/dispatcher-weakref')()

const kDestroyed = Symbol('destroyed')
const kClosed = Symbol('closed')
const kOnConnect = Symbol('onConnect')
const kOnDisconnect = Symbol('onDisconnect')
const kOnConnectionError = Symbol('onConnectionError')
const kMaxRedirections = Symbol('maxRedirections')
const kOnDrain = Symbol('onDrain')
const kFactory = Symbol('factory')
const kFinalizer = Symbol('finalizer')
const kOptions = Symbol('options')

function defaultFactory (origin, opts) {
  return opts && opts.connections === 1
    ? new Client(origin, opts)
    : new Pool(origin, opts)
}

class Agent extends Dispatcher {
  constructor ({ factory = defaultFactory, maxRedirections = 0, connect, ...options } = {}) {
    super()

    if (typeof factory !== 'function') {
      throw new InvalidArgumentError('factory must be a function.')
    }

    if (connect != null && typeof connect !== 'function' && typeof connect !== 'object') {
      throw new InvalidArgumentError('connect must be a function or an object')
    }

    if (!Number.isInteger(maxRedirections) || maxRedirections < 0) {
      throw new InvalidArgumentError('maxRedirections must be a positive number')
    }

    if (connect && typeof connect !== 'function') {
      connect = { ...connect }
    }

    this[kOptions] = { ...util.deepClone(options), connect }
    this[kMaxRedirections] = maxRedirections
    this[kFactory] = factory
    this[kClients] = new Map()
    this[kFinalizer] = new FinalizationRegistry(/* istanbul ignore next: gc is undeterministic */ key => {
      const ref = this[kClients].get(key)
      if (ref !== undefined && ref.deref() === undefined) {
        this[kClients].delete(key)
      }
    })
    this[kClosed] = false
    this[kDestroyed] = false

    const agent = this

    this[kOnDrain] = (origin, targets) => {
      agent.emit('drain', origin, [agent, ...targets])
    }

    this[kOnConnect] = (origin, targets) => {
      agent.emit('connect', origin, [agent, ...targets])
    }

    this[kOnDisconnect] = (origin, targets, err) => {
      agent.emit('disconnect', origin, [agent, ...targets], err)
    }

    this[kOnConnectionError] = (origin, targets, err) => {
      agent.emit('connectionError', origin, [agent, ...targets], err)
    }
  }

  get [kRunning] () {
    let ret = 0
    for (const ref of this[kClients].values()) {
      const client = ref.deref()
      /* istanbul ignore next: gc is undeterministic */
      if (client) {
        ret += client[kRunning]
      }
    }
    return ret
  }

  dispatch (opts, handler) {
    if (!handler || typeof handler !== 'object') {
      throw new InvalidArgumentError('handler must be an object.')
    }

    try {
      if (!opts || typeof opts !== 'object') {
        throw new InvalidArgumentError('opts must be an object.')
      }

      let key
      if (opts.origin && (typeof opts.origin === 'string' || opts.origin instanceof URL)) {
        key = String(opts.origin)
      } else {
        throw new InvalidArgumentError('opts.origin must be a non-empty string or URL.')
      }

      if (this[kDestroyed]) {
        throw new ClientDestroyedError()
      }

      if (this[kClosed]) {
        throw new ClientClosedError()
      }

      const ref = this[kClients].get(key)

      let dispatcher = ref ? ref.deref() : null
      if (!dispatcher) {
        dispatcher = this[kFactory](opts.origin, this[kOptions])
          .on('drain', this[kOnDrain])
          .on('connect', this[kOnConnect])
          .on('disconnect', this[kOnDisconnect])
          .on('connectionError', this[kOnConnectionError])

        this[kClients].set(key, new WeakRef(dispatcher))
        this[kFinalizer].register(dispatcher, key)
      }

      const { maxRedirections = this[kMaxRedirections] } = opts
      if (maxRedirections != null && maxRedirections !== 0) {
        opts = { ...opts, maxRedirections: 0 } // Stop sub dispatcher from also redirecting.
        handler = new RedirectHandler(this, maxRedirections, opts, handler)
      }

      return dispatcher.dispatch(opts, handler)
    } catch (err) {
      if (typeof handler.onError !== 'function') {
        throw new InvalidArgumentError('invalid onError method')
      }

      handler.onError(err)
    }
  }

  get closed () {
    return this[kClosed]
  }

  get destroyed () {
    return this[kDestroyed]
  }

  close (callback) {
    if (callback != null && typeof callback !== 'function') {
      throw new InvalidArgumentError('callback must be a function')
    }

    this[kClosed] = true

    const closePromises = []
    for (const ref of this[kClients].values()) {
      const client = ref.deref()
      /* istanbul ignore else: gc is undeterministic */
      if (client) {
        closePromises.push(client.close())
      }
    }

    if (!callback) {
      return Promise.all(closePromises)
    }

    // Should never error.
    Promise.all(closePromises).then(() => process.nextTick(callback))
  }

  destroy (err, callback) {
    if (typeof err === 'function') {
      callback = err
      err = null
    }

    if (callback != null && typeof callback !== 'function') {
      throw new InvalidArgumentError('callback must be a function')
    }

    this[kClosed] = true
    this[kDestroyed] = true

    const destroyPromises = []
    for (const ref of this[kClients].values()) {
      const client = ref.deref()
      /* istanbul ignore else: gc is undeterministic */
      if (client) {
        destroyPromises.push(client.destroy(err))
      }
    }

    if (!callback) {
      return Promise.all(destroyPromises)
    }

    // Should never error.
    Promise.all(destroyPromises).then(() => process.nextTick(callback))
  }
}

module.exports = Agent
