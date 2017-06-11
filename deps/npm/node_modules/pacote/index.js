'use strict'

module.exports = {
  extract: require('./extract'),
  manifest: require('./manifest'),
  prefetch: require('./prefetch'),
  clearMemoized: require('./lib/fetch').clearMemoized
}
