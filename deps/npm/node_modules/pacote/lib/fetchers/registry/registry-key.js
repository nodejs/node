'use strict'

const url = require('url')

// Called a nerf dart in the main codebase. Used as a "safe"
// key when fetching registry info from config.
module.exports = registryKey
function registryKey (registry) {
  const parsed = url.parse(registry)
  const formatted = url.format({
    host: parsed.host,
    pathname: parsed.pathname,
    slashes: parsed.slashes
  })
  return url.resolve(formatted, '.')
}
