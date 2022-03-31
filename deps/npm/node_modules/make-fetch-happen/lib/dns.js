const LRUCache = require('lru-cache')
const dns = require('dns')

const defaultOptions = exports.defaultOptions = {
  family: undefined,
  hints: dns.ADDRCONFIG,
  all: false,
  verbatim: undefined,
}

const lookupCache = exports.lookupCache = new LRUCache({ max: 50 })

// this is a factory so that each request can have its own opts (i.e. ttl)
// while still sharing the cache across all requests
exports.getLookup = (dnsOptions) => {
  return (hostname, options, callback) => {
    if (typeof options === 'function') {
      callback = options
      options = null
    } else if (typeof options === 'number') {
      options = { family: options }
    }

    options = { ...defaultOptions, ...options }

    const key = JSON.stringify({
      hostname,
      family: options.family,
      hints: options.hints,
      all: options.all,
      verbatim: options.verbatim,
    })

    if (lookupCache.has(key)) {
      const [address, family] = lookupCache.get(key)
      process.nextTick(callback, null, address, family)
      return
    }

    dnsOptions.lookup(hostname, options, (err, address, family) => {
      if (err) {
        return callback(err)
      }

      lookupCache.set(key, [address, family], { ttl: dnsOptions.ttl })
      return callback(null, address, family)
    })
  }
}
