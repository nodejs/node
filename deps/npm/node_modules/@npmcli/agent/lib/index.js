'use strict'

const { LRUCache } = require('lru-cache')
const { urlify, cacheAgent } = require('./util')
const { normalizeOptions, cacheOptions } = require('./options')
const { getProxy, proxyCache } = require('./proxy.js')
const dns = require('./dns.js')
const { HttpAgent, HttpsAgent } = require('./agents.js')

const agentCache = new LRUCache({ max: 20 })

const getAgent = (url, { agent: _agent, proxy: _proxy, noProxy, ..._options } = {}) => {
  // false has meaning so this can't be a simple truthiness check
  if (_agent != null) {
    return _agent
  }

  url = urlify(url)

  const secure = url.protocol === 'https:'
  const proxy = getProxy(url, { proxy: _proxy, noProxy })
  const options = { ...normalizeOptions(_options), proxy }

  return cacheAgent({
    key: cacheOptions({ ...options, secure }),
    cache: agentCache,
    secure,
    proxies: [HttpAgent, HttpsAgent],
  }, options)
}

module.exports = {
  getAgent,
  HttpAgent,
  HttpsAgent,
  cache: {
    proxy: proxyCache,
    agent: agentCache,
    dns: dns.cache,
    clear: () => {
      proxyCache.clear()
      agentCache.clear()
      dns.cache.clear()
    },
  },
}
