'use strict'

const pudding = require('figgy-pudding')

const NpmHooksConfig = pudding()

module.exports = config
function config (opts) {
  return NpmHooksConfig.apply(
    null,
    [opts, opts.config].concat([].slice.call(arguments, 1))
  )
}
