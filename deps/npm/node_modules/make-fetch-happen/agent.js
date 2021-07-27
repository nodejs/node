'use strict'
const LRU = require('lru-cache')
const url = require('url')

let AGENT_CACHE = new LRU({ max: 50 })
let HttpsAgent
let HttpAgent

module.exports = getAgent

function getAgent (uri, opts) {
  const parsedUri = url.parse(typeof uri === 'string' ? uri : uri.url)
  const isHttps = parsedUri.protocol === 'https:'
  const pxuri = getProxyUri(uri, opts)

  const key = [
    `https:${isHttps}`,
    pxuri
      ? `proxy:${pxuri.protocol}//${pxuri.host}:${pxuri.port}`
      : '>no-proxy<',
    `local-address:${opts.localAddress || '>no-local-address<'}`,
    `strict-ssl:${isHttps ? !!opts.strictSSL : '>no-strict-ssl<'}`,
    `ca:${(isHttps && opts.ca) || '>no-ca<'}`,
    `cert:${(isHttps && opts.cert) || '>no-cert<'}`,
    `key:${(isHttps && opts.key) || '>no-key<'}`
  ].join(':')

  if (opts.agent != null) { // `agent: false` has special behavior!
    return opts.agent
  }

  if (AGENT_CACHE.peek(key)) {
    return AGENT_CACHE.get(key)
  }

  if (pxuri) {
    const proxy = getProxy(pxuri, opts, isHttps)
    AGENT_CACHE.set(key, proxy)
    return proxy
  }

  if (isHttps && !HttpsAgent) {
    HttpsAgent = require('agentkeepalive').HttpsAgent
  } else if (!isHttps && !HttpAgent) {
    HttpAgent = require('agentkeepalive')
  }

  // If opts.timeout is zero, set the agentTimeout to zero as well. A timeout
  // of zero disables the timeout behavior (OS limits still apply). Else, if
  // opts.timeout is a non-zero value, set it to timeout + 1, to ensure that
  // the node-fetch-npm timeout will always fire first, giving us more
  // consistent errors.
  const agentTimeout = opts.timeout === 0 ? 0 : opts.timeout + 1

  const agent = isHttps ? new HttpsAgent({
    maxSockets: opts.maxSockets || 15,
    ca: opts.ca,
    cert: opts.cert,
    key: opts.key,
    localAddress: opts.localAddress,
    rejectUnauthorized: opts.strictSSL,
    timeout: agentTimeout
  }) : new HttpAgent({
    maxSockets: opts.maxSockets || 15,
    localAddress: opts.localAddress,
    timeout: agentTimeout
  })
  AGENT_CACHE.set(key, agent)
  return agent
}

function checkNoProxy (uri, opts) {
  const host = url.parse(uri).hostname.split('.').reverse()
  let noproxy = (opts.noProxy || getProcessEnv('no_proxy'))
  if (typeof noproxy === 'string') {
    noproxy = noproxy.split(/\s*,\s*/g)
  }
  return noproxy && noproxy.some(no => {
    const noParts = no.split('.').filter(x => x).reverse()
    if (!noParts.length) { return false }
    for (let i = 0; i < noParts.length; i++) {
      if (host[i] !== noParts[i]) {
        return false
      }
    }
    return true
  })
}

module.exports.getProcessEnv = getProcessEnv

function getProcessEnv (env) {
  if (!env) { return }

  let value

  if (Array.isArray(env)) {
    for (let e of env) {
      value = process.env[e] ||
        process.env[e.toUpperCase()] ||
        process.env[e.toLowerCase()]
      if (typeof value !== 'undefined') { break }
    }
  }

  if (typeof env === 'string') {
    value = process.env[env] ||
      process.env[env.toUpperCase()] ||
      process.env[env.toLowerCase()]
  }

  return value
}

function getProxyUri (uri, opts) {
  const protocol = url.parse(uri).protocol

  const proxy = opts.proxy || (
    protocol === 'https:' && getProcessEnv('https_proxy')
  ) || (
      protocol === 'http:' && getProcessEnv(['https_proxy', 'http_proxy', 'proxy'])
    )
  if (!proxy) { return null }

  const parsedProxy = (typeof proxy === 'string') ? url.parse(proxy) : proxy

  return !checkNoProxy(uri, opts) && parsedProxy
}

let HttpProxyAgent
let HttpsProxyAgent
let SocksProxyAgent
function getProxy (proxyUrl, opts, isHttps) {
  let popts = {
    host: proxyUrl.hostname,
    port: proxyUrl.port,
    protocol: proxyUrl.protocol,
    path: proxyUrl.path,
    auth: proxyUrl.auth,
    ca: opts.ca,
    cert: opts.cert,
    key: opts.key,
    timeout: opts.timeout === 0 ? 0 : opts.timeout + 1,
    localAddress: opts.localAddress,
    maxSockets: opts.maxSockets || 15,
    rejectUnauthorized: opts.strictSSL
  }

  if (proxyUrl.protocol === 'http:' || proxyUrl.protocol === 'https:') {
    if (!isHttps) {
      if (!HttpProxyAgent) {
        HttpProxyAgent = require('http-proxy-agent')
      }

      return new HttpProxyAgent(popts)
    } else {
      if (!HttpsProxyAgent) {
        HttpsProxyAgent = require('https-proxy-agent')
      }

      return new HttpsProxyAgent(popts)
    }
  }
  if (proxyUrl.protocol.startsWith('socks')) {
    if (!SocksProxyAgent) {
      SocksProxyAgent = require('socks-proxy-agent')
    }

    return new SocksProxyAgent(popts)
  }
}
