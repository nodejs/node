'use strict'

const { PoolStats } = require('../util/stats.js')
const DispatcherBase = require('./dispatcher-base')
const FixedQueue = require('./fixed-queue')
const { kConnected, kSize, kRunning, kPending, kQueued, kBusy, kFree, kUrl, kClose, kDestroy, kDispatch } = require('../core/symbols')

const kClients = Symbol('clients')
const kNeedDrain = Symbol('needDrain')
const kQueue = Symbol('queue')
const kClosedResolve = Symbol('closed resolve')
const kOnDrain = Symbol('onDrain')
const kOnConnect = Symbol('onConnect')
const kOnDisconnect = Symbol('onDisconnect')
const kOnConnectionError = Symbol('onConnectionError')
const kOnClientBusy = Symbol('on client busy')
const kOnClientDrain = Symbol('on client drain')
const kDrainQueue = Symbol('drain queue')
const kGetDispatcher = Symbol('get dispatcher')
const kHasDispatcher = Symbol('has dispatcher')
const kAddClient = Symbol('add client')
const kRemoveClient = Symbol('remove client')
const kRetireClient = Symbol('retire client')
const kRetiring = Symbol('retiring clients')

// Closes every live client, including clients that have been
// taken out of rotation but are still finishing their own
// requests.
function closeClients (pool) {
  const closeAll = []
  for (let i = 0; i < pool[kClients].length; i++) {
    const client = pool[kClients][i]
    if (!client.destroyed) {
      closeAll.push(client.close())
    }
  }
  for (const closed of pool[kRetiring].values()) {
    closeAll.push(closed)
  }
  return Promise.all(closeAll)
}

class PoolBase extends DispatcherBase {
  [kQueue] = new FixedQueue();

  [kQueued] = 0;

  [kClients] = [];

  // Clients removed from kClients that have not finished closing,
  // mapped to a promise that settles once they have.
  [kRetiring] = new Map();

  [kNeedDrain] = false;

  [kOnDrain] (client, origin, targets) {
    if (client.closed || client.destroyed) {
      return
    }

    const queue = this[kQueue]
    let needDrain = false

    this[kOnClientDrain](client)

    while (!needDrain) {
      const item = queue.shift()
      if (!item) {
        break
      }
      this[kQueued]--
      needDrain = !client.dispatch(item.opts, item.handler)
    }

    client[kNeedDrain] = needDrain
    if (needDrain) {
      this[kOnClientBusy](client)

      // This client is full but requests are still queued, e.g. an h2
      // client that has just released everything queued while ALPN was
      // negotiated and hit maxConcurrentStreams. Hand the rest to other
      // clients, creating new ones up to the connection limit, instead of
      // leaving them for this client to drain one batch at a time.
      if (!queue.isEmpty()) {
        return this[kDrainQueue](origin, targets)
      }
    }

    if (!needDrain && this[kNeedDrain]) {
      this[kNeedDrain] = false
      this.emit('drain', origin, [this, ...targets])
    }

    if (this[kClosedResolve] && queue.isEmpty()) {
      return closeClients(this).then(this[kClosedResolve])
    }
  }

  [kOnClientBusy] () {}

  [kOnClientDrain] () {}

  [kDrainQueue] (origin, targets) {
    const queue = this[kQueue]
    let hasDispatcher = true

    while (!queue.isEmpty()) {
      const dispatcher = this[kGetDispatcher]()
      if (!dispatcher) {
        hasDispatcher = false
        break
      }

      const item = queue.shift()
      this[kQueued]--

      if (!dispatcher.dispatch(item.opts, item.handler)) {
        dispatcher[kNeedDrain] = true
        this[kOnClientBusy](dispatcher)
        hasDispatcher = this[kHasDispatcher]()
        if (!hasDispatcher) {
          break
        }
      }
    }

    if (hasDispatcher && this[kNeedDrain]) {
      this[kNeedDrain] = false
      this.emit('drain', origin, [this, ...targets])
    }

    if (this[kClosedResolve] && queue.isEmpty()) {
      return closeClients(this).then(this[kClosedResolve])
    }
  }

  [kOnConnect] = (origin, targets) => {
    this.emit('connect', origin, [this, ...targets])
  };

  [kOnDisconnect] = (origin, targets, err) => {
    this.emit('disconnect', origin, [this, ...targets], err)
  };

  [kOnConnectionError] = (origin, targets, err) => {
    this.emit('connectionError', origin, [this, ...targets], err)
  }

  get [kBusy] () {
    return this[kNeedDrain]
  }

  get [kConnected] () {
    let ret = 0
    for (const { [kConnected]: connected } of this[kClients]) {
      ret += connected
    }
    return ret
  }

  get [kFree] () {
    let ret = 0
    for (const { [kConnected]: connected, [kNeedDrain]: needDrain } of this[kClients]) {
      ret += connected && !needDrain
    }
    return ret
  }

  get [kPending] () {
    let ret = this[kQueued]
    for (const { [kPending]: pending } of this[kClients]) {
      ret += pending
    }
    return ret
  }

  get [kRunning] () {
    let ret = 0
    for (const { [kRunning]: running } of this[kClients]) {
      ret += running
    }
    return ret
  }

  get [kSize] () {
    let ret = this[kQueued]
    for (const { [kSize]: size } of this[kClients]) {
      ret += size
    }
    return ret
  }

  get stats () {
    return new PoolStats(this)
  }

  [kClose] () {
    if (this[kQueue].isEmpty()) {
      return closeClients(this)
    } else {
      return new Promise((resolve) => {
        this[kClosedResolve] = resolve
      })
    }
  }

  [kDestroy] (err) {
    while (true) {
      const item = this[kQueue].shift()
      if (!item) {
        break
      }
      item.handler.onResponseError(null, err)
    }

    const destroyAll = new Array(this[kClients].length)
    for (let i = 0; i < this[kClients].length; i++) {
      destroyAll[i] = this[kClients][i].destroy(err)
    }
    for (const client of this[kRetiring].keys()) {
      destroyAll.push(client.destroy(err))
    }
    return Promise.all(destroyAll)
  }

  [kDispatch] (opts, handler) {
    const dispatcher = this[kGetDispatcher]()

    if (!dispatcher) {
      this[kNeedDrain] = true
      this[kQueue].push({ opts, handler })
      this[kQueued]++
    } else if (!dispatcher.dispatch(opts, handler)) {
      dispatcher[kNeedDrain] = true
      this[kOnClientBusy](dispatcher)
      this[kNeedDrain] = !this[kHasDispatcher]()
    }

    return !this[kNeedDrain]
  }

  [kHasDispatcher] () {
    for (let i = 0; i < this[kClients].length; i++) {
      const dispatcher = this[kClients][i]

      if (
        !dispatcher[kNeedDrain] &&
        dispatcher.closed !== true &&
        dispatcher.destroyed !== true
      ) {
        return true
      }
    }

    return false
  }

  [kAddClient] (client) {
    client
      .on('drain', this[kOnDrain].bind(this, client))
      .on('connect', this[kOnConnect])
      .on('disconnect', this[kOnDisconnect])
      .on('connectionError', this[kOnConnectionError])

    this[kClients].push(client)

    if (this[kNeedDrain]) {
      queueMicrotask(() => {
        if (this[kNeedDrain] && !client[kNeedDrain]) {
          this[kOnDrain](client, client[kUrl], [client, this])
        }
      })
    }

    return this
  }

  // Takes a client out of rotation. A closed client no longer takes requests
  // from the pool queue (see kOnDrain), but it finishes the requests it
  // already has. Until it has, pool.close() waits for it and pool.destroy()
  // destroys it.
  [kRetireClient] (client) {
    const idx = this[kClients].indexOf(client)
    if (idx !== -1) {
      this[kClients].splice(idx, 1)
    }

    if (client.destroyed || this[kRetiring].has(client)) {
      return
    }

    // Use the callback form: custom dispatchers from `factory` are not
    // required to return a promise, and may call back synchronously.
    let done = false
    let resolveClosed
    const closed = new Promise((resolve) => { resolveClosed = resolve })
    this[kRetiring].set(client, closed)
    client.close(() => {
      if (!done) {
        done = true
        this[kRetiring].delete(client)
        resolveClosed()
      }
    })
  }

  [kRemoveClient] (client) {
    this[kRetireClient](client)

    this[kNeedDrain] = !this[kClients].some(dispatcher => (
      !dispatcher[kNeedDrain] &&
      dispatcher.closed !== true &&
      dispatcher.destroyed !== true
    ))
  }
}

module.exports = {
  PoolBase,
  kClients,
  kNeedDrain,
  kAddClient,
  kRemoveClient,
  kRetireClient,
  kDrainQueue,
  kOnClientBusy,
  kOnClientDrain,
  kGetDispatcher,
  kHasDispatcher
}
