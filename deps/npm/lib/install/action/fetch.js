'use strict'

const BB = require('bluebird')

const finished = BB.promisify(require('mississippi').finished)
const npmConfig = require('../../config/figgy-config.js')
const packageId = require('../../utils/package-id.js')
const pacote = require('pacote')

module.exports = fetch
function fetch (staging, pkg, log, next) {
  log.silly('fetch', packageId(pkg))
  const opts = npmConfig({integrity: pkg.package._integrity})
  return finished(pacote.tarball.stream(pkg.package._requested, opts))
    .then(() => next(), next)
}
