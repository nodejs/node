'use strict'

const isHeaderConditional = require('./is-header-conditional')
// Default cacache-based cache
const Cache = require('../cache')

module.exports = function initializeCache (opts) {
  /**
   * NOTE: `opts.cacheManager` is the path to cache
   * We're making the assumption that if `opts.cacheManager` *isn't* a string,
   * it's a cache object
   */
  if (typeof opts.cacheManager === 'string') {
    // Need to make a cache object
    opts.cacheManager = new Cache(opts.cacheManager, opts)
  }

  opts.cache = opts.cache || 'default'

  if (opts.cache === 'default' && isHeaderConditional(opts.headers)) {
    // If header list contains `If-Modified-Since`, `If-None-Match`,
    // `If-Unmodified-Since`, `If-Match`, or `If-Range`, fetch will set cache
    // mode to "no-store" if it is "default".
    opts.cache = 'no-store'
  }
}
