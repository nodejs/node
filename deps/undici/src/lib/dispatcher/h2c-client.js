'use strict'
const { connect } = require('node:net')

const { kClose, kDestroy } = require('../core/symbols')
const { InvalidArgumentError } = require('../core/errors')
const util = require('../core/util')

const Client = require('./client')
const DispatcherBase = require('./dispatcher-base')

class H2CClient extends DispatcherBase {
  #client = null

  constructor (origin, clientOpts) {
    if (typeof origin === 'string') {
      origin = new URL(origin)
    }

    if (origin.protocol !== 'http:') {
      throw new InvalidArgumentError(
        'h2c-client: Only h2c protocol is supported'
      )
    }

    const { connect, maxConcurrentStreams, pipelining, ...opts } =
      clientOpts ?? {}
    let defaultMaxConcurrentStreams = 100
    let defaultPipelining = 100

    if (
      maxConcurrentStreams != null &&
      Number.isInteger(maxConcurrentStreams) &&
      maxConcurrentStreams > 0
    ) {
      defaultMaxConcurrentStreams = maxConcurrentStreams
    }

    if (pipelining != null && Number.isInteger(pipelining) && pipelining > 0) {
      defaultPipelining = pipelining
    }

    if (defaultPipelining > defaultMaxConcurrentStreams) {
      throw new InvalidArgumentError(
        'h2c-client: pipelining cannot be greater than maxConcurrentStreams'
      )
    }

    super()

    this.#client = new Client(origin, {
      ...opts,
      connect: this.#buildConnector(connect),
      maxConcurrentStreams: defaultMaxConcurrentStreams,
      pipelining: defaultPipelining,
      allowH2: true
    })
  }

  #buildConnector (connectOpts) {
    return (opts, callback) => {
      const timeout = connectOpts?.connectOpts ?? 10e3
      const { hostname, port, pathname } = opts
      const socket = connect({
        ...opts,
        host: hostname,
        port,
        pathname
      })

      // Set TCP keep alive options on the socket here instead of in connect() for the case of assigning the socket
      if (opts.keepAlive == null || opts.keepAlive) {
        const keepAliveInitialDelay =
          opts.keepAliveInitialDelay == null ? 60e3 : opts.keepAliveInitialDelay
        socket.setKeepAlive(true, keepAliveInitialDelay)
      }

      socket.alpnProtocol = 'h2'

      const clearConnectTimeout = util.setupConnectTimeout(
        new WeakRef(socket),
        { timeout, hostname, port }
      )

      socket
        .setNoDelay(true)
        .once('connect', function () {
          queueMicrotask(clearConnectTimeout)

          if (callback) {
            const cb = callback
            callback = null
            cb(null, this)
          }
        })
        .on('error', function (err) {
          queueMicrotask(clearConnectTimeout)

          if (callback) {
            const cb = callback
            callback = null
            cb(err)
          }
        })

      return socket
    }
  }

  dispatch (opts, handler) {
    return this.#client.dispatch(opts, handler)
  }

  [kClose] () {
    return this.#client.close()
  }

  [kDestroy] () {
    return this.#client.destroy()
  }
}

module.exports = H2CClient
