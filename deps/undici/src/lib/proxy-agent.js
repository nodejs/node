'use strict'

const { kProxy, kClose, kDestroy } = require('./core/symbols')
const Agent = require('./agent')
const DispatcherBase = require('./dispatcher-base')
const { InvalidArgumentError } = require('./core/errors')

const kAgent = Symbol('proxy agent')

class ProxyAgent extends DispatcherBase {
  constructor (opts) {
    super(opts)
    this[kProxy] = buildProxyOptions(opts)
    this[kAgent] = new Agent(opts)
  }

  dispatch (opts, handler) {
    const { host } = new URL(opts.origin)
    return this[kAgent].dispatch(
      {
        ...opts,
        origin: this[kProxy].uri,
        path: opts.origin + opts.path,
        headers: {
          ...buildHeaders(opts.headers),
          host
        }
      },
      handler
    )
  }

  async [kClose] () {
    await this[kAgent].close()
  }

  async [kDestroy] () {
    await this[kAgent].destroy()
  }
}

function buildProxyOptions (opts) {
  if (typeof opts === 'string') {
    opts = { uri: opts }
  }

  if (!opts || !opts.uri) {
    throw new InvalidArgumentError('Proxy opts.uri is mandatory')
  }

  return {
    uri: opts.uri,
    protocol: opts.protocol || 'https'
  }
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

module.exports = ProxyAgent
