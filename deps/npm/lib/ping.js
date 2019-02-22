'use strict'

const npmConfig = require('./config/figgy-config.js')
const fetch = require('libnpm/fetch')
const figgyPudding = require('figgy-pudding')
const log = require('npmlog')
const npm = require('./npm.js')
const output = require('./utils/output.js')

const PingConfig = figgyPudding({
  json: {},
  registry: {}
})

module.exports = ping

ping.usage = 'npm ping\nping registry'

function ping (args, silent, cb) {
  if (typeof cb !== 'function') {
    cb = silent
    silent = false
  }

  const opts = PingConfig(npmConfig())
  const registry = opts.registry
  log.notice('PING', registry)
  const start = Date.now()
  return fetch('/-/ping?write=true', opts).then(
    res => res.json().catch(() => ({}))
  ).then(details => {
    if (silent) {
    } else {
      const time = Date.now() - start
      log.notice('PONG', `${time / 1000}ms`)
      if (npm.config.get('json')) {
        output(JSON.stringify({
          registry,
          time,
          details
        }, null, 2))
      } else if (Object.keys(details).length) {
        log.notice('PONG', `${JSON.stringify(details, null, 2)}`)
      }
    }
  }).nodeify(cb)
}
