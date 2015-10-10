var mkdir = require('mkdirp')
var assert = require('assert')
var log = require('npmlog')
var path = require('path')
var sha = require('sha')
var retry = require('retry')
var createWriteStream = require('fs-write-stream-atomic')
var npm = require('../npm.js')
var inflight = require('inflight')
var addLocalTarball = require('./add-local-tarball.js')
var cacheFile = require('npm-cache-filename')
var rimraf = require('rimraf')
var pulseTillDone = require('../utils/pulse-till-done.js')

module.exports = addRemoteTarball

function addRemoteTarball (u, pkgData, shasum, auth, cb_) {
  assert(typeof u === 'string', 'must have module URL')
  assert(typeof cb_ === 'function', 'must have callback')

  function cb (er, data) {
    if (data) {
      data._from = u
      data._resolved = u
      data._shasum = data._shasum || shasum
    }
    cb_(er, data)
  }

  cb_ = inflight(u, cb_)
  if (!cb_) return log.verbose('addRemoteTarball', u, 'already in flight; waiting')
  log.verbose('addRemoteTarball', u, 'not in flight; adding')

  // XXX Fetch direct to cache location, store tarballs under
  // ${cache}/registry.npmjs.org/pkg/-/pkg-1.2.3.tgz
  var tmp = cacheFile(npm.tmp, u)

  function next (er, resp, shasum) {
    if (er) return cb(er)
    addLocalTarball(tmp, pkgData, shasum, cleanup)
  }
  function cleanup (er, data) {
    if (er) return cb(er)
    rimraf(tmp, function () {
      cb(er, data)
    })
  }

  log.verbose('addRemoteTarball', [u, shasum])
  mkdir(path.dirname(tmp), function (er) {
    if (er) return cb(er)
    addRemoteTarball_(u, tmp, shasum, auth, next)
  })
}

function addRemoteTarball_ (u, tmp, shasum, auth, cb) {
  // Tuned to spread 3 attempts over about a minute.
  // See formula at <https://github.com/tim-kos/node-retry>.
  var operation = retry.operation({
    retries: npm.config.get('fetch-retries'),
    factor: npm.config.get('fetch-retry-factor'),
    minTimeout: npm.config.get('fetch-retry-mintimeout'),
    maxTimeout: npm.config.get('fetch-retry-maxtimeout')
  })

  operation.attempt(function (currentAttempt) {
    log.info(
      'retry',
      'fetch attempt', currentAttempt,
      'at', (new Date()).toLocaleTimeString()
    )
    fetchAndShaCheck(u, tmp, shasum, auth, function (er, response, shasum) {
      // Only retry on 408, 5xx or no `response`.
      var sc = response && response.statusCode
      var statusRetry = !sc || (sc === 408 || sc >= 500)
      if (er && statusRetry && operation.retry(er)) {
        log.warn('retry', 'will retry, error on last attempt: ' + er)
        return
      }
      cb(er, response, shasum)
    })
  })
}

function fetchAndShaCheck (u, tmp, shasum, auth, cb) {
  cb = pulseTillDone('fetchTarball', cb)
  npm.registry.fetch(u, { auth: auth }, function (er, response) {
    if (er) {
      log.error('fetch failed', u)
      return cb(er, response)
    }

    var tarball = createWriteStream(tmp, { mode: npm.modes.file })
    tarball.on('error', function (er) {
      cb(er)
      tarball.destroy()
    })

    tarball.on('finish', function () {
      if (!shasum) {
        // Well, we weren't given a shasum, so at least sha what we have
        // in case we want to compare it to something else later
        return sha.get(tmp, function (er, shasum) {
          log.silly('fetchAndShaCheck', 'shasum', shasum)
          cb(er, response, shasum)
        })
      }

      // validate that the url we just downloaded matches the expected shasum.
      log.silly('fetchAndShaCheck', 'shasum', shasum)
      sha.check(tmp, shasum, function (er) {
        if (er && er.message) {
          // add original filename for better debuggability
          er.message = er.message + '\n' + 'From:     ' + u
        }
        return cb(er, response, shasum)
      })
    })

    response.pipe(tarball)
  })
}
