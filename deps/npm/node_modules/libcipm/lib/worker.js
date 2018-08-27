'use strict'

const BB = require('bluebird')

const log = require('npmlog')
const pacote = require('pacote')

module.exports = (args, cb) => {
  const parsed = typeof args === 'string' ? JSON.parse(args) : args
  const spec = parsed[0]
  const extractTo = parsed[1]
  const opts = parsed[2]
  opts.log = log
  log.level = opts.loglevel
  return BB.resolve(pacote.extract(spec, extractTo, opts)).nodeify(cb)
}
