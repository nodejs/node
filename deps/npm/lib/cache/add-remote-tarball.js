var mkdir = require("mkdirp")
  , assert = require("assert")
  , log = require("npmlog")
  , path = require("path")
  , sha = require("sha")
  , retry = require("retry")
  , npm = require("../npm.js")
  , fetch = require("../utils/fetch.js")
  , inflight = require("inflight")
  , locker = require("../utils/locker.js")
  , lock = locker.lock
  , unlock = locker.unlock
  , addLocalTarball = require("./add-local-tarball.js")
  , cacheFile = require("npm-cache-filename")

module.exports = addRemoteTarball

function addRemoteTarball (u, pkgData, shasum, cb_) {
  assert(typeof u === "string", "must have module URL")
  assert(typeof cb_ === "function", "must have callback")

  function cb (er, data) {
    if (data) {
      data._from = u
      data._shasum = data._shasum || shasum
      data._resolved = u
    }
    unlock(u, function () {
      cb_(er, data)
    })
  }

  cb_ = inflight(u, cb_)

  if (!cb_) return

  // XXX Fetch direct to cache location, store tarballs under
  // ${cache}/registry.npmjs.org/pkg/-/pkg-1.2.3.tgz
  var tmp = cacheFile(npm.tmp, u)

  function next (er, resp, shasum) {
    if (er) return cb(er)
    addLocalTarball(tmp, pkgData, shasum, cb)
  }

  lock(u, function (er) {
    if (er) return cb(er)

    log.verbose("addRemoteTarball", [u, shasum])
    mkdir(path.dirname(tmp), function (er) {
      if (er) return cb(er)
      addRemoteTarball_(u, tmp, shasum, next)
    })
  })
}

function addRemoteTarball_(u, tmp, shasum, cb) {
  // Tuned to spread 3 attempts over about a minute.
  // See formula at <https://github.com/tim-kos/node-retry>.
  var operation = retry.operation
    ( { retries: npm.config.get("fetch-retries")
      , factor: npm.config.get("fetch-retry-factor")
      , minTimeout: npm.config.get("fetch-retry-mintimeout")
      , maxTimeout: npm.config.get("fetch-retry-maxtimeout") })

  operation.attempt(function (currentAttempt) {
    log.info("retry", "fetch attempt " + currentAttempt
      + " at " + (new Date()).toLocaleTimeString())
    fetchAndShaCheck(u, tmp, shasum, function (er, response, shasum) {
      // Only retry on 408, 5xx or no `response`.
      var sc = response && response.statusCode
      var statusRetry = !sc || (sc === 408 || sc >= 500)
      if (er && statusRetry && operation.retry(er)) {
        log.info("retry", "will retry, error on last attempt: " + er)
        return
      }
      cb(er, response, shasum)
    })
  })
}

function fetchAndShaCheck (u, tmp, shasum, cb) {
  fetch(u, tmp, function (er, response) {
    if (er) {
      log.error("fetch failed", u)
      return cb(er, response)
    }

    if (!shasum) {
      // Well, we weren't given a shasum, so at least sha what we have
      // in case we want to compare it to something else later
      return sha.get(tmp, function (er, shasum) {
        cb(er, response, shasum)
      })
    }

    // validate that the url we just downloaded matches the expected shasum.
    sha.check(tmp, shasum, function (er) {
      if (er && er.message) {
        // add original filename for better debuggability
        er.message = er.message + '\n' + 'From:     ' + u
      }
      return cb(er, response, shasum)
    })
  })
}
