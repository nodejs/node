'use strict'

const http = require('http')
const https = require('https')
const net = require('net')
const tls = require('tls')
const { once } = require('events')
const { createTimeout, abortRace, urlify, appendPort, cacheAgent } = require('./util')
const { normalizeOptions, cacheOptions } = require('./options')
const { getProxy, getProxyType, proxyCache } = require('./proxy.js')
const Errors = require('./errors.js')

const createAgent = (base, name) => {
  const SECURE = base === https
  const SOCKET_TYPE = SECURE ? tls : net

  const agent = class extends base.Agent {
    #options
    #timeouts
    #proxy
    #socket

    constructor (_options) {
      const { timeouts, proxy, noProxy, ...options } = normalizeOptions(_options)

      super(options)

      this.#options = options
      this.#timeouts = timeouts
      this.#proxy = proxy ? { proxies: getProxyType(proxy), proxy: urlify(proxy), noProxy } : null
    }

    get proxy () {
      return this.#proxy ? { url: this.#proxy.proxy } : {}
    }

    #getProxy (options) {
      const proxy = this.#proxy
        ? getProxy(appendPort(`${options.protocol}//${options.host}`, options.port), this.#proxy)
        : null

      if (!proxy) {
        return
      }

      return cacheAgent({
        key: cacheOptions({
          ...options,
          ...this.#options,
          secure: SECURE,
          timeouts: this.#timeouts,
          proxy,
        }),
        cache: proxyCache,
        secure: SECURE,
        proxies: this.#proxy.proxies,
      }, proxy, this.#options)
    }

    #setKeepAlive (socket) {
      socket.setKeepAlive(this.keepAlive, this.keepAliveMsecs)
      socket.setNoDelay(this.keepAlive)
    }

    #setIdleTimeout (socket, options) {
      if (this.#timeouts.idle) {
        socket.setTimeout(this.#timeouts.idle, () => {
          socket.destroy(new Errors.IdleTimeoutError(options))
        })
      }
    }

    async #proxyConnect (proxy, request, options) {
      // socks-proxy-agent accepts a dns lookup function
      options.lookup ??= this.#options.lookup

      // all the proxy agents use this secureEndpoint option to determine
      // if the proxy should connect over tls or not. we can set it based
      // on if the HttpAgent or HttpsAgent is used.
      options.secureEndpoint = SECURE

      const socket = await abortRace([
        (ac) => createTimeout(this.#timeouts.connection, ac).catch(() => {
          throw new Errors.ConnectionTimeoutError(options)
        }),
        (ac) => proxy.connect(request, options).then((s) => {
          this.#setKeepAlive(s)

          const connectEvent = SECURE ? 'secureConnect' : 'connect'
          const connectingEvent = SECURE ? 'secureConnecting' : 'connecting'

          if (!s[connectingEvent]) {
            return s
          }

          return abortRace([
            () => once(s, 'error', ac).then((err) => {
              throw err
            }),
            () => once(s, connectEvent, ac).then(() => s),
          ], ac)
        }),
      ])

      this.#setIdleTimeout(socket, options)

      return socket
    }

    async connect (request, options) {
      const proxy = this.#getProxy(options)
      if (proxy) {
        return this.#proxyConnect(proxy, request, options)
      }

      const socket = SOCKET_TYPE.connect(options)

      this.#setKeepAlive(socket)

      await abortRace([
        (s) => createTimeout(this.#timeouts.connection, s).catch(() => {
          throw new Errors.ConnectionTimeoutError(options)
        }),
        (s) => once(socket, 'error', s).then((err) => {
          throw err
        }),
        (s) => once(socket, 'connect', s),
      ])

      this.#setIdleTimeout(socket, options)

      return socket
    }

    addRequest (request, options) {
      const proxy = this.#getProxy(options)
      // it would be better to call proxy.addRequest here but this causes the
      // http-proxy-agent to call its super.addRequest which causes the request
      // to be added to the agent twice. since we only support 3 agents
      // currently (see the required agents in proxy.js) we have manually
      // checked that the only public methods we need to call are called in the
      // next block. this could change in the future and presumably we would get
      // failing tests until we have properly called the necessary methods on
      // each of our proxy agents
      if (proxy?.setRequestProps) {
        proxy.setRequestProps(request, options)
      }

      request.setHeader('connection', this.keepAlive ? 'keep-alive' : 'close')

      const responseTimeout = createTimeout(this.#timeouts.response)
      if (responseTimeout) {
        request.once('finish', () => {
          responseTimeout.start(() => {
            request.destroy(new Errors.ResponseTimeoutError(request, this.proxy?.url))
          })
        })
        request.once('response', () => {
          responseTimeout.clear()
        })
      }

      const transferTimeout = createTimeout(this.#timeouts.transfer)
      if (transferTimeout) {
        request.once('response', (res) => {
          transferTimeout.start(() => {
            res.destroy(new Errors.TransferTimeoutError(request, this.proxy?.url))
          })
          res.once('close', () => {
            transferTimeout.clear()
          })
        })
      }

      return super.addRequest(request, options)
    }

    createSocket (req, options, cb) {
      return Promise.resolve()
        .then(() => this.connect(req, options))
        .then((socket) => {
          this.#socket = socket
          return super.createSocket(req, options, cb)
        }, cb)
    }

    createConnection () {
      return this.#socket
    }
  }

  Object.defineProperty(agent, 'name', { value: name })
  return agent
}

module.exports = {
  HttpAgent: createAgent(http, 'HttpAgent'),
  HttpsAgent: createAgent(https, 'HttpsAgent'),
}
