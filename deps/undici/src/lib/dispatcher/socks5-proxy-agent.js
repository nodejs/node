'use strict'

const { URL } = require('node:url')

let tls // include tls conditionally since it is not always available
const DispatcherBase = require('./dispatcher-base')
const { InvalidArgumentError } = require('../core/errors')
const { Socks5Client, STATES } = require('../core/socks5-client')
const { kDispatch, kClose, kDestroy } = require('../core/symbols')
const Pool = require('./pool')
const buildConnector = require('../core/connect')
const { debuglog } = require('node:util')

const debug = debuglog('undici:socks5-proxy')

const kProxyUrl = Symbol('proxy url')
const kProxyHeaders = Symbol('proxy headers')
const kProxyAuth = Symbol('proxy auth')
const kProxyProtocol = Symbol('proxy protocol')
const kPools = Symbol('pools')
const kConnector = Symbol('connector')
const kRequestTls = Symbol('request tls settings')

// Static flag to ensure warning is only emitted once per process
let experimentalWarningEmitted = false

/**
 * SOCKS5 proxy agent for dispatching requests through a SOCKS5 proxy
 */
class Socks5ProxyAgent extends DispatcherBase {
  constructor (proxyUrl, options = {}) {
    super()

    // Emit experimental warning only once
    if (!experimentalWarningEmitted) {
      process.emitWarning(
        'SOCKS5 proxy support is experimental and subject to change',
        'ExperimentalWarning'
      )
      experimentalWarningEmitted = true
    }

    if (!proxyUrl) {
      throw new InvalidArgumentError('Proxy URL is mandatory')
    }

    // Parse proxy URL
    const url = typeof proxyUrl === 'string' ? new URL(proxyUrl) : proxyUrl

    if (url.protocol !== 'socks5:' && url.protocol !== 'socks:') {
      throw new InvalidArgumentError('Proxy URL must use socks5:// or socks:// protocol')
    }

    this[kProxyUrl] = url
    this[kProxyHeaders] = options.headers || {}
    this[kProxyProtocol] = options.proxyTls ? 'https:' : 'http:'
    this[kRequestTls] = options.requestTls

    // Extract auth from URL or options
    this[kProxyAuth] = {
      username: options.username || (url.username ? decodeURIComponent(url.username) : null),
      password: options.password || (url.password ? decodeURIComponent(url.password) : null)
    }

    // Create connector for proxy connection
    this[kConnector] = options.connect || buildConnector({
      ...options.proxyTls,
      servername: options.proxyTls?.servername || url.hostname
    })

    // Pools for the actual HTTP connections (with SOCKS5 tunnel connect function), keyed by origin
    this[kPools] = new Map()
  }

  /**
   * Create a SOCKS5 connection to the proxy
   */
  async createSocks5Connection (targetHost, targetPort) {
    const proxyHost = this[kProxyUrl].hostname
    const proxyPort = parseInt(this[kProxyUrl].port) || 1080

    debug('creating SOCKS5 connection to', proxyHost, proxyPort)

    // Connect to the SOCKS5 proxy
    const socket = await new Promise((resolve, reject) => {
      this[kConnector]({
        hostname: proxyHost,
        host: proxyHost,
        port: proxyPort,
        protocol: this[kProxyProtocol]
      }, (err, socket) => {
        if (err) {
          reject(err)
        } else {
          resolve(socket)
        }
      })
    })

    // Create SOCKS5 client
    const socks5Client = new Socks5Client(socket, this[kProxyAuth])

    // Handle SOCKS5 errors
    socks5Client.on('error', (err) => {
      debug('SOCKS5 error:', err)
      socket.destroy()
    })

    // Perform SOCKS5 handshake
    await socks5Client.handshake()

    // Wait for authentication (if required)
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        reject(new Error('SOCKS5 authentication timeout'))
      }, 5000)

      const onAuthenticated = () => {
        clearTimeout(timeout)
        socks5Client.removeListener('error', onError)
        resolve()
      }

      const onError = (err) => {
        clearTimeout(timeout)
        socks5Client.removeListener('authenticated', onAuthenticated)
        reject(err)
      }

      // Check if already authenticated (for NO_AUTH method)
      if (socks5Client.state === STATES.AUTHENTICATED) {
        clearTimeout(timeout)
        resolve()
      } else {
        socks5Client.once('authenticated', onAuthenticated)
        socks5Client.once('error', onError)
      }
    })

    // Send CONNECT command
    await socks5Client.connect(targetHost, targetPort)

    // Wait for connection
    await new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        reject(new Error('SOCKS5 connection timeout'))
      }, 5000)

      const onConnected = (info) => {
        debug('SOCKS5 tunnel established to', targetHost, targetPort, 'via', info)
        clearTimeout(timeout)
        socks5Client.removeListener('error', onError)
        resolve()
      }

      const onError = (err) => {
        clearTimeout(timeout)
        socks5Client.removeListener('connected', onConnected)
        reject(err)
      }

      socks5Client.once('connected', onConnected)
      socks5Client.once('error', onError)
    })

    return socket
  }

  /**
   * Dispatch a request through the SOCKS5 proxy
   */
  [kDispatch] (opts, handler) {
    const { origin } = opts

    debug('dispatching request to', origin, 'via SOCKS5')

    try {
      const originKey = String(origin)
      let pool = this[kPools].get(originKey)
      // Create a Pool per origin so requests are not routed to the wrong host
      if (!pool || pool.destroyed || pool.closed) {
        pool = new Pool(origin, {
          pipelining: opts.pipelining,
          connections: opts.connections,
          connect: async (connectOpts, callback) => {
            try {
              const url = new URL(origin)
              const targetHost = url.hostname
              const targetPort = parseInt(url.port) || (url.protocol === 'https:' ? 443 : 80)

              debug('establishing SOCKS5 connection to', targetHost, targetPort)

              // Create SOCKS5 tunnel
              const socket = await this.createSocks5Connection(targetHost, targetPort)

              // Handle TLS if needed
              let finalSocket = socket
              if (url.protocol === 'https:') {
                if (!tls) {
                  tls = require('node:tls')
                }
                debug('upgrading to TLS')
                finalSocket = tls.connect({
                  ...this[kRequestTls],
                  socket,
                  servername: this[kRequestTls]?.servername || targetHost
                })

                await new Promise((resolve, reject) => {
                  finalSocket.once('secureConnect', resolve)
                  finalSocket.once('error', reject)
                })
              }

              callback(null, finalSocket)
            } catch (err) {
              debug('SOCKS5 connection error:', err)
              callback(err)
            }
          }
        })
        this[kPools].set(originKey, pool)
      }

      // Dispatch the request through the per-origin pool
      return pool[kDispatch](opts, handler)
    } catch (err) {
      debug('dispatch error:', err)
      if (typeof handler.onResponseError === 'function') {
        handler.onResponseError(null, err)
        return false
      } else if (typeof handler.onError === 'function') {
        handler.onError(err)
        return false
      } else {
        throw err
      }
    }
  }

  async [kClose] () {
    const closePromises = []
    for (const pool of this[kPools].values()) {
      closePromises.push(pool.close())
    }
    this[kPools].clear()
    await Promise.all(closePromises)
  }

  async [kDestroy] (err) {
    const destroyPromises = []
    for (const pool of this[kPools].values()) {
      destroyPromises.push(pool.destroy(err))
    }
    this[kPools].clear()
    await Promise.all(destroyPromises)
  }
}

module.exports = Socks5ProxyAgent
