'use strict'

const BB = require('bluebird')

const extract = require('pacote/extract')
const npmlog = require('npmlog')

module.exports = (args, cb) => {
  const parsed = typeof args === 'string' ? JSON.parse(args) : args
  const spec = parsed[0]
  const extractTo = parsed[1]
  const opts = parsed[2]
  if (!opts.log) {
    opts.log = npmlog
  }
  opts.log.level = opts.loglevel || opts.log.level
  BB.resolve(extract(spec, extractTo, opts)).nodeify(cb)
}
