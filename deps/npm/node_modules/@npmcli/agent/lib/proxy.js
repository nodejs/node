'use strict'

const { HttpProxyAgent } = require('http-proxy-agent')
const { HttpsProxyAgent } = require('https-proxy-agent')
const { SocksProxyAgent } = require('socks-proxy-agent')
const { LRUCache } = require('lru-cache')
const { InvalidProxyProtocolError } = require('./errors.js')
const { urlify } = require('./util.js')

const PROXY_CACHE = new LRUCache({ max: 20 })

const PROXY_ENV = (() => {
  const keys = new Set(['https_proxy', 'http_proxy', 'proxy', 'no_proxy'])
  const values = {}
  for (let [key, value] of Object.entries(process.env)) {
    key = key.toLowerCase()
    if (keys.has(key)) {
      values[key] = value
    }
  }
  return values
})()

const SOCKS_PROTOCOLS = new Set(SocksProxyAgent.protocols)

const getProxyType = (url) => {
  url = urlify(url)

  const protocol = url.protocol.slice(0, -1)
  if (SOCKS_PROTOCOLS.has(protocol)) {
    return [SocksProxyAgent]
  }
  if (protocol === 'https' || protocol === 'http') {
    return [HttpProxyAgent, HttpsProxyAgent]
  }

  throw new InvalidProxyProtocolError(url)
}

const isNoProxy = (url, noProxy) => {
  if (typeof noProxy === 'string') {
    noProxy = noProxy.split(',').map((p) => p.trim()).filter(Boolean)
  }

  if (!noProxy || !noProxy.length) {
    return false
  }

  const hostSegments = url.hostname.split('.').reverse()

  return noProxy.some((no) => {
    const noSegments = no.split('.').filter(Boolean).reverse()
    if (!noSegments.length) {
      return false
    }

    for (let i = 0; i < noSegments.length; i++) {
      if (hostSegments[i] !== noSegments[i]) {
        return false
      }
    }

    return true
  })
}

const getProxy = (url, {
  proxy = PROXY_ENV.https_proxy,
  noProxy = PROXY_ENV.no_proxy,
}) => {
  url = urlify(url)

  if (!proxy && url.protocol !== 'https:') {
    proxy = PROXY_ENV.http_proxy || PROXY_ENV.proxy
  }

  if (!proxy || isNoProxy(url, noProxy)) {
    return null
  }

  return urlify(proxy)
}

module.exports = {
  getProxyType,
  getProxy,
  proxyCache: PROXY_CACHE,
}
