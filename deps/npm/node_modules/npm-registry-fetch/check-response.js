'use strict'

const config = require('./config.js')
const errors = require('./errors.js')
const LRU = require('lru-cache')

module.exports = checkResponse
function checkResponse (method, res, registry, startTime, opts) {
  opts = config(opts)
  if (res.headers.has('npm-notice') && !res.headers.has('x-local-cache')) {
    opts.get('log').notice('', res.headers.get('npm-notice'))
  }
  checkWarnings(res, registry, opts)
  if (res.status >= 400) {
    logRequest(method, res, startTime, opts)
    return checkErrors(method, res, startTime, opts)
  } else {
    res.body.on('end', () => logRequest(method, res, startTime, opts))
    return res
  }
}

function logRequest (method, res, startTime, opts) {
  const elapsedTime = Date.now() - startTime
  const attempt = res.headers.get('x-fetch-attempts')
  const attemptStr = attempt && attempt > 1 ? ` attempt #${attempt}` : ''
  const cacheStr = res.headers.get('x-local-cache') ? ' (from cache)' : ''
  opts.get('log').http(
    'fetch',
    `${method.toUpperCase()} ${res.status} ${res.url} ${elapsedTime}ms${attemptStr}${cacheStr}`
  )
}

const WARNING_REGEXP = /^\s*(\d{3})\s+(\S+)\s+"(.*)"\s+"([^"]+)"/
const BAD_HOSTS = new LRU({ max: 50 })

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
        opts.get('log').warn('registry', `Using stale data from ${registry} because the host is inaccessible -- are you offline?`)
      } else {
        opts.get('log').warn('registry', `Unexpected warning for ${registry}: ${warnings['199'].message}`)
      }
    }
    if (warnings['111']) {
      // 111 Revalidation failed -- we're using stale data
      opts.get('log').warn(
        'registry',
        `Using stale data from ${registry} due to a request error during revalidation.`
      )
    }
  }
}

function checkErrors (method, res, startTime, opts) {
  return res.buffer()
    .catch(() => null)
    .then(body => {
      try {
        body = JSON.parse(body.toString('utf8'))
      } catch (e) {}
      if (res.status === 401 && res.headers.get('www-authenticate')) {
        const auth = res.headers.get('www-authenticate')
          .split(/,\s*/)
          .map(s => s.toLowerCase())
        if (auth.indexOf('ipaddress') !== -1) {
          throw new errors.HttpErrorAuthIPAddress(
            method, res, body, opts.spec
          )
        } else if (auth.indexOf('otp') !== -1) {
          throw new errors.HttpErrorAuthOTP(
            method, res, body, opts.spec
          )
        } else {
          throw new errors.HttpErrorAuthUnknown(
            method, res, body, opts.spec
          )
        }
      } else {
        throw new errors.HttpErrorGeneral(
          method, res, body, opts.spec
        )
      }
    })
}
