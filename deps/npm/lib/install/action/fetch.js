'use strict'

const packageId = require('../../utils/package-id.js')
const pacote = require('pacote')
const pacoteOpts = require('../../config/pacote')

module.exports = fetch
function fetch (staging, pkg, log, next) {
  log.silly('fetch', packageId(pkg))
  const opts = pacoteOpts({integrity: pkg.package._integrity})
  pacote.prefetch(pkg.package._requested, opts).then(() => next(), next)
}
