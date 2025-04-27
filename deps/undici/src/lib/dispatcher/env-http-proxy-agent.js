'use strict'

const DispatcherBase = require('./dispatcher-base')
const { kClose, kDestroy, kClosed, kDestroyed, kDispatch, kNoProxyAgent, kHttpProxyAgent, kHttpsProxyAgent } = require('../core/symbols')
const ProxyAgent = require('./proxy-agent')
const Agent = require('./agent')

const DEFAULT_PORTS = {
  'http:': 80,
  'https:': 443
}

class EnvHttpProxyAgent extends DispatcherBase {
  #noProxyValue = null
  #noProxyEntries = null
  #opts = null

  constructor (opts = {}) {
    super()
    this.#opts = opts

    const { httpProxy, httpsProxy, noProxy, ...agentOpts } = opts

    this[kNoProxyAgent] = new Agent(agentOpts)

    const HTTP_PROXY = httpProxy ?? process.env.http_proxy ?? process.env.HTTP_PROXY
    if (HTTP_PROXY) {
      this[kHttpProxyAgent] = new ProxyAgent({ ...agentOpts, uri: HTTP_PROXY })
    } else {
      this[kHttpProxyAgent] = this[kNoProxyAgent]
    }

    const HTTPS_PROXY = httpsProxy ?? process.env.https_proxy ?? process.env.HTTPS_PROXY
    if (HTTPS_PROXY) {
      this[kHttpsProxyAgent] = new ProxyAgent({ ...agentOpts, uri: HTTPS_PROXY })
    } else {
      this[kHttpsProxyAgent] = this[kHttpProxyAgent]
    }

    this.#parseNoProxy()
  }

  [kDispatch] (opts, handler) {
    const url = new URL(opts.origin)
    const agent = this.#getProxyAgentForUrl(url)
    return agent.dispatch(opts, handler)
  }

  async [kClose] () {
    await this[kNoProxyAgent].close()
    if (!this[kHttpProxyAgent][kClosed]) {
      await this[kHttpProxyAgent].close()
    }
    if (!this[kHttpsProxyAgent][kClosed]) {
      await this[kHttpsProxyAgent].close()
    }
  }

  async [kDestroy] (err) {
    await this[kNoProxyAgent].destroy(err)
    if (!this[kHttpProxyAgent][kDestroyed]) {
      await this[kHttpProxyAgent].destroy(err)
    }
    if (!this[kHttpsProxyAgent][kDestroyed]) {
      await this[kHttpsProxyAgent].destroy(err)
    }
  }

  #getProxyAgentForUrl (url) {
    let { protocol, host: hostname, port } = url

    // Stripping ports in this way instead of using parsedUrl.hostname to make
    // sure that the brackets around IPv6 addresses are kept.
    hostname = hostname.replace(/:\d*$/, '').toLowerCase()
    port = Number.parseInt(port, 10) || DEFAULT_PORTS[protocol] || 0
    if (!this.#shouldProxy(hostname, port)) {
      return this[kNoProxyAgent]
    }
    if (protocol === 'https:') {
      return this[kHttpsProxyAgent]
    }
    return this[kHttpProxyAgent]
  }

  #shouldProxy (hostname, port) {
    if (this.#noProxyChanged) {
      this.#parseNoProxy()
    }

    if (this.#noProxyEntries.length === 0) {
      return true // Always proxy if NO_PROXY is not set or empty.
    }
    if (this.#noProxyValue === '*') {
      return false // Never proxy if wildcard is set.
    }

    for (let i = 0; i < this.#noProxyEntries.length; i++) {
      const entry = this.#noProxyEntries[i]
      if (entry.port && entry.port !== port) {
        continue // Skip if ports don't match.
      }
      if (!/^[.*]/.test(entry.hostname)) {
        // No wildcards, so don't proxy only if there is not an exact match.
        if (hostname === entry.hostname) {
          return false
        }
      } else {
        // Don't proxy if the hostname ends with the no_proxy host.
        if (hostname.endsWith(entry.hostname.replace(/^\*/, ''))) {
          return false
        }
      }
    }

    return true
  }

  #parseNoProxy () {
    const noProxyValue = this.#opts.noProxy ?? this.#noProxyEnv
    const noProxySplit = noProxyValue.split(/[,\s]/)
    const noProxyEntries = []

    for (let i = 0; i < noProxySplit.length; i++) {
      const entry = noProxySplit[i]
      if (!entry) {
        continue
      }
      const parsed = entry.match(/^(.+):(\d+)$/)
      noProxyEntries.push({
        hostname: (parsed ? parsed[1] : entry).toLowerCase(),
        port: parsed ? Number.parseInt(parsed[2], 10) : 0
      })
    }

    this.#noProxyValue = noProxyValue
    this.#noProxyEntries = noProxyEntries
  }

  get #noProxyChanged () {
    if (this.#opts.noProxy !== undefined) {
      return false
    }
    return this.#noProxyValue !== this.#noProxyEnv
  }

  get #noProxyEnv () {
    return process.env.no_proxy ?? process.env.NO_PROXY ?? ''
  }
}

module.exports = EnvHttpProxyAgent
