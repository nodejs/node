'use strict'

const { URL } = require('node:url')

let tls // include tls conditionally since it is not always available
const DispatcherBase = require('./dispatcher-base')
const { ConnectTimeoutError, InvalidArgumentError } = require('../core/errors')
const { Socks5Client, STATES } = require('../core/socks5-client')
const { kBusy, kConnected, kDispatch, kClose, kDestroy } = require('../core/symbols')
const Pool = require('./pool')
const buildConnector = require('../core/connect')
const { setupConnectTimeout } = require('../core/util')
const { debuglog } = require('node:util')

const debug = debuglog('undici:socks5-proxy')

const DEFAULT_SOCKS5_CONNECT_TIMEOUT = 5000

const kProxyUrl = Symbol('proxy url')
const kProxyHeaders = Symbol('proxy headers')
const kProxyAuth = Symbol('proxy auth')
const kProxyProtocol = Symbol('proxy protocol')
const kPools = Symbol('pools')
const kConnector = Symbol('connector')
const kConnectTimeout = Symbol('connect timeout')
const kRequestTls = Symbol('request tls settings')
const kRequestTlsTimeout = Symbol('request tls timeout')

function createConnectTimeoutError (hostname, port, timeout) {
  return new ConnectTimeoutError(
    `Connect Timeout Error (attempted address: ${hostname}:${port}, timeout: ${timeout}ms)`
  )
}

// Static flag to ensure warning is only emitted once per process
let experimentalWarningEmitted = false

/**
 * SOCKS5 proxy agent for dispatching requests through a SOCKS5 proxy
 */
class Socks5ProxyAgent extends DispatcherBase {
  constructor (proxyUrl, options = {}) {
    super(options)

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

    const connectTimeout = options.connectTimeout ?? DEFAULT_SOCKS5_CONNECT_TIMEOUT
    if (!Number.isFinite(connectTimeout) || connectTimeout < 0) {
      throw new InvalidArgumentError('invalid connectTimeout')
    }
    this[kConnectTimeout] = connectTimeout

    const { timeout, ...requestTls } = options.requestTls || {}
    const requestTlsTimeout = timeout ?? connectTimeout
    if (!Number.isFinite(requestTlsTimeout) || requestTlsTimeout < 0) {
      throw new InvalidArgumentError('invalid requestTls.timeout')
    }
    this[kRequestTls] = requestTls
    this[kRequestTlsTimeout] = requestTlsTimeout

    // Extract auth from URL or options
    this[kProxyAuth] = {
      username: options.username || (url.username ? decodeURIComponent(url.username) : null),
      password: options.password || (url.password ? decodeURIComponent(url.password) : null)
    }

    // Create connector for proxy connection
    const proxyTlsTimeout = options.proxyTls?.timeout ?? connectTimeout
    if (!Number.isFinite(proxyTlsTimeout) || proxyTlsTimeout < 0) {
      throw new InvalidArgumentError('invalid proxyTls.timeout')
    }
    this[kConnector] = options.connect || buildConnector({
      ...options.proxyTls,
      timeout: proxyTlsTimeout,
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
    const socketReady = Promise.withResolvers()

    this[kConnector]({
      hostname: proxyHost,
      host: proxyHost,
      port: proxyPort,
      protocol: this[kProxyProtocol]
    }, (err, socket) => {
      if (err) {
        socketReady.reject(err)
      } else {
        socketReady.resolve(socket)
      }
    })

    const socket = await socketReady.promise

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
    const authenticationReady = Promise.withResolvers()
    const authenticationTimeout = this[kConnectTimeout] === 0
      ? null
      : setTimeout(() => {
        cleanupAuthenticationListeners()
        socks5Client.destroy()
        authenticationReady.reject(
          createConnectTimeoutError(proxyHost, proxyPort, this[kConnectTimeout])
        )
      }, this[kConnectTimeout])

    const cleanupAuthenticationListeners = () => {
      clearTimeout(authenticationTimeout)
      socks5Client.removeListener('authenticated', onAuthenticated)
      socks5Client.removeListener('error', onAuthenticationError)
    }

    const onAuthenticated = () => {
      cleanupAuthenticationListeners()
      authenticationReady.resolve()
    }

    const onAuthenticationError = (err) => {
      cleanupAuthenticationListeners()
      authenticationReady.reject(err)
    }

    // Check if already authenticated (for NO_AUTH method)
    if (socks5Client.state === STATES.AUTHENTICATED) {
      clearTimeout(authenticationTimeout)
      authenticationReady.resolve()
    } else {
      socks5Client.once('authenticated', onAuthenticated)
      socks5Client.once('error', onAuthenticationError)
    }

    await authenticationReady.promise

    // Send CONNECT command
    await socks5Client.connect(targetHost, targetPort)

    // Wait for connection
    const connectionReady = Promise.withResolvers()
    const connectionTimeout = this[kConnectTimeout] === 0
      ? null
      : setTimeout(() => {
        cleanupConnectionListeners()
        socks5Client.destroy()
        connectionReady.reject(
          createConnectTimeoutError(targetHost, targetPort, this[kConnectTimeout])
        )
      }, this[kConnectTimeout])

    const cleanupConnectionListeners = () => {
      clearTimeout(connectionTimeout)
      socks5Client.removeListener('connected', onConnected)
      socks5Client.removeListener('error', onConnectionError)
    }

    const onConnected = (info) => {
      debug('SOCKS5 tunnel established to', targetHost, targetPort, 'via', info)
      cleanupConnectionListeners()
      connectionReady.resolve()
    }

    const onConnectionError = (err) => {
      cleanupConnectionListeners()
      connectionReady.reject(err)
    }

    socks5Client.once('connected', onConnected)
    socks5Client.once('error', onConnectionError)

    await connectionReady.promise

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

                const tlsReady = Promise.withResolvers()

                const cleanupTlsListeners = () => {
                  queueMicrotask(clearTlsTimeout)
                  finalSocket.removeListener('secureConnect', onSecureConnect)
                  finalSocket.removeListener('error', onTlsError)
                }

                const onSecureConnect = () => {
                  cleanupTlsListeners()
                  tlsReady.resolve()
                }

                const onTlsError = (err) => {
                  cleanupTlsListeners()
                  tlsReady.reject(err)
                }

                const clearTlsTimeout = setupConnectTimeout(new WeakRef(finalSocket), {
                  timeout: this[kRequestTlsTimeout],
                  hostname: targetHost,
                  port: targetPort
                })

                finalSocket.once('secureConnect', onSecureConnect)
                finalSocket.once('error', onTlsError)
                await tlsReady.promise
              }

              callback(null, finalSocket)
            } catch (err) {
              debug('SOCKS5 connection error:', err)
              callback(err)
            }
          }
        })
        this[kPools].set(originKey, pool)

        const closePoolIfUnused = () => {
          if (this[kPools].get(originKey) !== pool || pool[kConnected] > 0 || pool[kBusy]) {
            return
          }

          this[kPools].delete(originKey)
          if (!pool.destroyed) {
            pool.close()
          }
        }

        pool.on('disconnect', closePoolIfUnused)
        pool.on('connectionError', closePoolIfUnused)
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
