'use strict'

const initializeCache = require('./initialize-cache')

module.exports = function configureOptions (_opts) {
  const opts = Object.assign({}, _opts || {})
  opts.method = (opts.method || 'GET').toUpperCase()

  if (!opts.retry) {
    // opts.retry was falsy; set default
    opts.retry = { retries: 0 }
  } else {
    if (typeof opts.retry !== 'object') {
      // Shorthand
      if (typeof opts.retry === 'number')
        opts.retry = { retries: opts.retry }

      if (typeof opts.retry === 'string') {
        const value = parseInt(opts.retry, 10)
        opts.retry = (value) ? { retries: value } : { retries: 0 }
      }
    } else {
      // Set default retries
      opts.retry = Object.assign({}, { retries: 0 }, opts.retry)
    }
  }

  if (opts.cacheManager)
    initializeCache(opts)

  return opts
}
