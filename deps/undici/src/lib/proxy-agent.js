'use strict'

const { kProxy, kClose, kDestroy } = require('./core/symbols')
const Client = require('./agent')
const Agent = require('./agent')
const DispatcherBase = require('./dispatcher-base')
const { InvalidArgumentError } = require('./core/errors')
const buildConnector = require('./core/connect')

const kAgent = Symbol('proxy agent')
const kClient = Symbol('proxy client')
const kProxyHeaders = Symbol('proxy headers')
const kRequestTls = Symbol('request tls settings')
const kProxyTls = Symbol('proxy tls settings')
const kConnectEndpoint = Symbol('connect endpoint function')

class ProxyAgent extends DispatcherBase {
  constructor (opts) {
    super(opts)
    this[kProxy] = buildProxyOptions(opts)
    this[kRequestTls] = opts.requestTls
    this[kProxyTls] = opts.proxyTls
    this[kProxyHeaders] = {}

    if (opts.auth) {
      this[kProxyHeaders]['proxy-authorization'] = `Basic ${opts.auth}`
    }

    const connect = buildConnector({ ...opts.proxyTls })
    this[kConnectEndpoint] = buildConnector({ ...opts.requestTls })
    this[kClient] = new Client({ origin: opts.origin, connect })
    this[kAgent] = new Agent({ ...opts, connect: this.connectTunnel.bind(this) })
  }

  dispatch (opts, handler) {
    const { host } = new URL(opts.origin)
    const headers = buildHeaders(opts.headers)
    throwIfProxyAuthIsSent(headers)
    return this[kAgent].dispatch(
      {
        ...opts,
        headers: {
          ...headers,
          host
        }
      },
      handler
    )
  }

  async connectTunnel (opts, callback) {
    try {
      const { socket } = await this[kClient].connect({
        origin: this[kProxy].origin,
        port: this[kProxy].port,
        path: opts.host,
        signal: opts.signal,
        headers: {
          ...this[kProxyHeaders],
          host: opts.host
        },
        httpTunnel: true
      })
      if (opts.protocol !== 'https:') {
        callback(null, socket)
        return
      }
      let servername
      if (this[kRequestTls]) {
        servername = this[kRequestTls].servername
      } else {
        servername = opts.servername
      }
      this[kConnectEndpoint]({ ...opts, servername, httpSocket: socket }, callback)
    } catch (err) {
      callback(err)
    }
  }

  async [kClose] () {
    await this[kAgent].close()
    await this[kClient].close()
  }

  async [kDestroy] () {
    await this[kAgent].destroy()
    await this[kClient].destroy()
  }
}

function buildProxyOptions (opts) {
  if (typeof opts === 'string') {
    opts = { uri: opts }
  }

  if (!opts || !opts.uri) {
    throw new InvalidArgumentError('Proxy opts.uri is mandatory')
  }

  return new URL(opts.uri)
}

/**
 * @param {string[] | Record<string, string>} headers
 * @returns {Record<string, string>}
 */
function buildHeaders (headers) {
  // When using undici.fetch, the headers list is stored
  // as an array.
  if (Array.isArray(headers)) {
    /** @type {Record<string, string>} */
    const headersPair = {}

    for (let i = 0; i < headers.length; i += 2) {
      headersPair[headers[i]] = headers[i + 1]
    }

    return headersPair
  }

  return headers
}

/**
 * @param {Record<string, string>} headers
 *
 * Previous versions of ProxyAgent suggests the Proxy-Authorization in request headers
 * Nevertheless, it was changed and to avoid a security vulnerability by end users
 * this check was created.
 * It should be removed in the next major version for performance reasons
 */
function throwIfProxyAuthIsSent (headers) {
  const existProxyAuth = headers && Object.keys(headers)
    .find((key) => key.toLowerCase() === 'proxy-authorization')
  if (existProxyAuth) {
    throw new InvalidArgumentError('Proxy-Authorization should be sent in ProxyAgent constructor')
  }
}

module.exports = ProxyAgent
