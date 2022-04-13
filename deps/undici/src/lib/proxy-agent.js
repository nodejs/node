'use strict'

const { kProxy, kClose, kDestroy } = require('./core/symbols')
const { URL } = require('url')
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
          ...opts.headers,
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

module.exports = ProxyAgent
