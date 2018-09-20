'use strict'

const LRU = require('lru-cache')

const WARNING_REGEXP = /^\s*(\d{3})\s+(\S+)\s+"(.*)"\s+"([^"]+)"/
const BAD_HOSTS = new LRU({ max: 50 })

module.exports = checkWarnings
function checkWarnings (res, registry, opts) {
  if (res.headers.has('warning') && !BAD_HOSTS.has(registry)) {
    const warnings = {}
    res.headers.raw()['warning'].forEach(w => {
      const match = w.match(WARNING_REGEXP)
      if (match) {
        warnings[match[1]] = {
          code: match[1],
          host: match[2],
          message: match[3],
          date: new Date(match[4])
        }
      }
    })
    BAD_HOSTS.set(registry, true)
    if (warnings['199']) {
      if (warnings['199'].message.match(/ENOTFOUND/)) {
        opts.log.warn('registry', `Using stale data from ${registry} because the host is inaccessible -- are you offline?`)
      } else {
        opts.log.warn('registry', `Unexpected warning for ${registry}: ${warnings['199'].message}`)
      }
    }
    if (warnings['111']) {
      // 111 Revalidation failed -- we're using stale data
      opts.log.warn(
        'registry',
        `Using stale package data from ${registry} due to a request error during revalidation.`
      )
    }
  }
}
