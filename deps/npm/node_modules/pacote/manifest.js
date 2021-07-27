'use strict'

const fetchManifest = require('./lib/fetch').manifest
const finalizeManifest = require('./lib/finalize-manifest')
const optCheck = require('./lib/util/opt-check')
const pinflight = require('promise-inflight')
const npa = require('npm-package-arg')

module.exports = manifest
function manifest (spec, opts) {
  opts = optCheck(opts)
  spec = npa(spec, opts.where)

  const label = [
    spec.name,
    spec.saveSpec || spec.fetchSpec,
    spec.type,
    opts.cache,
    opts.registry,
    opts.scope
  ].join(':')
  return pinflight(label, () => {
    const startTime = Date.now()
    return fetchManifest(spec, opts).then(rawManifest => {
      return finalizeManifest(rawManifest, spec, opts)
    }).then(manifest => {
      if (opts.annotate) {
        manifest._from = spec.saveSpec || spec.raw
        manifest._requested = spec
        manifest._spec = spec.raw
        manifest._where = opts.where
      }
      const elapsedTime = Date.now() - startTime
      opts.log.silly('pacote', `${spec.type} manifest for ${spec.name}@${spec.saveSpec || spec.fetchSpec} fetched in ${elapsedTime}ms`)
      return manifest
    })
  })
}
