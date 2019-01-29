'use strict'

module.exports = {
  extract: require('./extract'),
  manifest: require('./manifest'),
  packument: require('./packument'),
  prefetch: require('./prefetch'),
  tarball: require('./tarball'),
  clearMemoized: require('./lib/fetch').clearMemoized
}
