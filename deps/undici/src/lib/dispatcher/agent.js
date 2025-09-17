'use strict'

const { InvalidArgumentError } = require('../core/errors')
const { kClients, kRunning, kClose, kDestroy, kDispatch, kUrl } = require('../core/symbols')
const DispatcherBase = require('./dispatcher-base')
const Pool = require('./pool')
const Client = require('./client')
const util = require('../core/util')

const kOnConnect = Symbol('onConnect')
const kOnDisconnect = Symbol('onDisconnect')
const kOnConnectionError = Symbol('onConnectionError')
const kOnDrain = Symbol('onDrain')
const kFactory = Symbol('factory')
const kOptions = Symbol('options')

function defaultFactory (origin, opts) {
  return opts && opts.connections === 1
    ? new Client(origin, opts)
    : new Pool(origin, opts)
}

class Agent extends DispatcherBase {
  constructor ({ factory = defaultFactory, connect, ...options } = {}) {
    if (typeof factory !== 'function') {
      throw new InvalidArgumentError('factory must be a function.')
    }

    if (connect != null && typeof connect !== 'function' && typeof connect !== 'object') {
      throw new InvalidArgumentError('connect must be a function or an object')
    }

    super()

    if (connect && typeof connect !== 'function') {
      connect = { ...connect }
    }

    this[kOptions] = { ...util.deepClone(options), connect }
    this[kFactory] = factory
    this[kClients] = new Map()

    this[kOnDrain] = (origin, targets) => {
      this.emit('drain', origin, [this, ...targets])
    }

    this[kOnConnect] = (origin, targets) => {
      const result = this[kClients].get(origin)
      if (result) {
        result.count += 1
      }
      this.emit('connect', origin, [this, ...targets])
    }

    this[kOnDisconnect] = (origin, targets, err) => {
      const result = this[kClients].get(origin)
      if (result) {
        result.count -= 1
        if (result.count <= 0) {
          this[kClients].delete(origin)
          result.dispatcher.destroy()
        }
      }
      this.emit('disconnect', origin, [this, ...targets], err)
    }

    this[kOnConnectionError] = (origin, targets, err) => {
      // TODO: should this decrement result.count here?
      this.emit('connectionError', origin, [this, ...targets], err)
    }
  }

  get [kRunning] () {
    let ret = 0
    for (const { dispatcher } of this[kClients].values()) {
      ret += dispatcher[kRunning]
    }
    return ret
  }

  [kDispatch] (opts, handler) {
    let key
    if (opts.origin && (typeof opts.origin === 'string' || opts.origin instanceof URL)) {
      key = String(opts.origin)
    } else {
      throw new InvalidArgumentError('opts.origin must be a non-empty string or URL.')
    }

    const result = this[kClients].get(key)
    let dispatcher = result && result.dispatcher
    if (!dispatcher) {
      dispatcher = this[kFactory](opts.origin, this[kOptions])
        .on('drain', this[kOnDrain])
        .on('connect', this[kOnConnect])
        .on('disconnect', this[kOnDisconnect])
        .on('connectionError', this[kOnConnectionError])

      this[kClients].set(key, { count: 0, dispatcher })
    }

    return dispatcher.dispatch(opts, handler)
  }

  async [kClose] () {
    const closePromises = []
    for (const { dispatcher } of this[kClients].values()) {
      closePromises.push(dispatcher.close())
    }
    this[kClients].clear()

    await Promise.all(closePromises)
  }

  async [kDestroy] (err) {
    const destroyPromises = []
    for (const { dispatcher } of this[kClients].values()) {
      destroyPromises.push(dispatcher.destroy(err))
    }
    this[kClients].clear()

    await Promise.all(destroyPromises)
  }

  get stats () {
    const allClientStats = {}
    for (const { dispatcher } of this[kClients].values()) {
      if (dispatcher.stats) {
        allClientStats[dispatcher[kUrl].origin] = dispatcher.stats
      }
    }
    return allClientStats
  }
}

module.exports = Agent
