'use strict'

const fetchPackument = require('./lib/fetch').packument
const optCheck = require('./lib/util/opt-check')
const pinflight = require('promise-inflight')
const npa = require('npm-package-arg')

module.exports = packument
function packument (spec, opts) {
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
  const startTime = Date.now()
  return pinflight(label, () => {
    return fetchPackument(spec, opts)
  }).then(p => {
    const elapsedTime = Date.now() - startTime
    opts.log.silly('pacote', `${spec.registry ? 'registry' : spec.type} packument for ${spec.name}@${spec.saveSpec || spec.fetchSpec} fetched in ${elapsedTime}ms`)
    return p
  })
}
