'use strict'

const timers = require('timers/promises')

const createKey = (obj) => {
  let key = ''
  const sorted = Object.entries(obj).sort((a, b) => a[0] - b[0])
  for (let [k, v] of sorted) {
    if (v == null) {
      v = 'null'
    } else if (v instanceof URL) {
      v = v.toString()
    } else if (typeof v === 'object') {
      v = createKey(v)
    }
    key += `${k}:${v}:`
  }
  return key
}

const createTimeout = (delay, signal) => {
  if (!delay) {
    return signal ? new Promise(() => {}) : null
  }

  if (!signal) {
    let timeout
    return {
      start: (cb) => (timeout = setTimeout(cb, delay)),
      clear: () => clearTimeout(timeout),
    }
  }

  return timers.setTimeout(delay, null, signal)
    .then(() => {
      throw new Error()
    }).catch((err) => {
      if (err.name === 'AbortError') {
        return
      }
      throw err
    })
}

const abortRace = async (promises, ac = new AbortController()) => {
  let res
  try {
    res = await Promise.race(promises.map((p) => p(ac)))
    ac.abort()
  } catch (err) {
    ac.abort()
    throw err
  }
  return res
}

const urlify = (url) => typeof url === 'string' ? new URL(url) : url

const appendPort = (host, port) => {
  // istanbul ignore next
  if (port) {
    host += `:${port}`
  }
  return host
}

const cacheAgent = ({ key, cache, secure, proxies }, ...args) => {
  if (cache.has(key)) {
    return cache.get(key)
  }
  const Ctor = (secure ? proxies[1] : proxies[0]) ?? proxies[0]
  const agent = new Ctor(...args)
  cache.set(key, agent)
  return agent
}

module.exports = {
  createKey,
  createTimeout,
  abortRace,
  urlify,
  cacheAgent,
  appendPort,
}
