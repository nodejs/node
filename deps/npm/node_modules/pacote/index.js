'use strict'

module.exports = {
  extract: require('./extract'),
  manifest: require('./manifest'),
  prefetch: require('./prefetch'),
  tarball: require('./tarball'),
  clearMemoized: require('./lib/fetch').clearMemoized
}
