var assert = require('assert')
var url = require('url')

var request = require('request')
var once = require('once')

module.exports = fetch

function fetch (uri, params, cb) {
  assert(typeof uri === 'string', 'must pass uri to request')
  assert(params && typeof params === 'object', 'must pass params to request')
  assert(typeof cb === 'function', 'must pass callback to request')

  cb = once(cb)

  var client = this
  this.attempt(function (operation) {
    makeRequest.call(client, uri, params, function (er, req) {
      if (er) return cb(er)

      req.once('error', retryOnError)

      function retryOnError (er) {
        if (operation.retry(er)) {
          client.log.info('retry', 'will retry, error on last attempt: ' + er)
        } else {
          cb(er)
        }
      }

      req.on('response', function (res) {
        client.log.http('fetch', '' + res.statusCode, uri)
        req.removeListener('error', retryOnError)

        var er
        var statusCode = res && res.statusCode
        if (statusCode === 200) {
          res.resume()

          req.once('error', function (er) {
            res.emit('error', er)
          })

          return cb(null, res)
          // Only retry on 408, 5xx or no `response`.
        } else if (statusCode === 408) {
          er = new Error('request timed out')
        } else if (statusCode >= 500) {
          er = new Error('server error ' + statusCode)
        }

        if (er && operation.retry(er)) {
          client.log.info('retry', 'will retry, error on last attempt: ' + er)
        } else {
          cb(new Error('fetch failed with status code ' + statusCode))
        }
      })
    })
  })
}

function makeRequest (remote, params, cb) {
  var parsed = url.parse(remote)
  this.log.http('fetch', 'GET', parsed.href)

  var headers = params.headers || {}
  var er = this.authify(
    params.auth && params.auth.alwaysAuth,
    parsed,
    headers,
    params.auth
  )
  if (er) return cb(er)

  var opts = this.initialize(
    parsed,
    'GET',
    'application/x-tar, application/vnd.github+json; q=0.1',
    headers
  )
  // always want to follow redirects for fetch
  opts.followRedirect = true

  cb(null, request(opts))
}
