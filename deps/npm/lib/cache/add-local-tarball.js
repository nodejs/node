var mkdir = require('mkdirp')
var assert = require('assert')
var fs = require('graceful-fs')
var writeFileAtomic = require('write-file-atomic')
var path = require('path')
var sha = require('sha')
var npm = require('../npm.js')
var log = require('npmlog')
var tar = require('../utils/tar.js')
var pathIsInside = require('path-is-inside')
var getCacheStat = require('./get-stat.js')
var cachedPackageRoot = require('./cached-package-root.js')
var chownr = require('chownr')
var inflight = require('inflight')
var once = require('once')
var writeStream = require('fs-write-stream-atomic')
var tempFilename = require('../utils/temp-filename.js')
var rimraf = require('rimraf')
var packageId = require('../utils/package-id.js')

module.exports = addLocalTarball

function addLocalTarball (p, pkgData, shasum, cb) {
  assert(typeof p === 'string', 'must have path')
  assert(typeof cb === 'function', 'must have callback')

  if (!pkgData) pkgData = {}

  // If we don't have a shasum yet, compute it.
  if (!shasum) {
    return sha.get(p, function (er, shasum) {
      if (er) return cb(er)
      log.silly('addLocalTarball', 'shasum (computed)', shasum)
      addLocalTarball(p, pkgData, shasum, cb)
    })
  }

  if (pathIsInside(p, npm.cache)) {
    if (path.basename(p) !== 'package.tgz') {
      return cb(new Error('Not a valid cache tarball name: ' + p))
    }
    log.verbose('addLocalTarball', 'adding from inside cache', p)
    return addPlacedTarball(p, pkgData, shasum, cb)
  }

  addTmpTarball(p, pkgData, shasum, function (er, data) {
    if (data) {
      data._resolved = p
      data._shasum = data._shasum || shasum
    }
    return cb(er, data)
  })
}

function addPlacedTarball (p, pkgData, shasum, cb) {
  assert(pkgData, 'should have package data by now')
  assert(typeof cb === 'function', 'cb function required')

  getCacheStat(function (er, cs) {
    if (er) return cb(er)
    return addPlacedTarball_(p, pkgData, cs.uid, cs.gid, shasum, cb)
  })
}

function addPlacedTarball_ (p, pkgData, uid, gid, resolvedSum, cb) {
  var folder = path.join(cachedPackageRoot(pkgData), 'package')

  // First, make sure we have the shasum, if we don't already.
  if (!resolvedSum) {
    sha.get(p, function (er, shasum) {
      if (er) return cb(er)
      addPlacedTarball_(p, pkgData, uid, gid, shasum, cb)
    })
    return
  }

  mkdir(folder, function (er) {
    if (er) return cb(er)
    var pj = path.join(folder, 'package.json')
    var json = JSON.stringify(pkgData, null, 2)
    writeFileAtomic(pj, json, function (er) {
      cb(er, pkgData)
    })
  })
}

function addTmpTarball (tgz, pkgData, shasum, cb) {
  assert(typeof cb === 'function', 'must have callback function')
  assert(shasum, 'must have shasum by now')

  cb = inflight('addTmpTarball:' + tgz, cb)
  if (!cb) return log.verbose('addTmpTarball', tgz, 'already in flight; not adding')
  log.verbose('addTmpTarball', tgz, 'not in flight; adding')

  // we already have the package info, so just move into place
  if (pkgData && pkgData.name && pkgData.version) {
    log.verbose(
      'addTmpTarball',
      'already have metadata; skipping unpack for',
      packageId(pkgData)
    )
    return addTmpTarball_(tgz, pkgData, shasum, cb)
  }

  // This is a tarball we probably downloaded from the internet.  The shasum's
  // already been checked, but we haven't ever had a peek inside, so we unpack
  // it here just to make sure it is what it says it is.
  //
  // NOTE: we might not have any clue what we think it is, for example if the
  // user just did `npm install ./foo.tgz`

  var target = tempFilename('unpack')
  getCacheStat(function (er, cs) {
    if (er) return cb(er)

    log.verbose('addTmpTarball', 'validating metadata from', tgz)
    tar.unpack(tgz, target, null, null, cs.uid, cs.gid, function (unpackEr, data) {
      // cleanup the extracted package and move on with the metadata
      rimraf(target, function () {
        if (unpackEr) return cb(unpackEr)
        // check that this is what we expected.
        if (!data.name) {
          return cb(new Error('No name provided'))
        } else if (pkgData.name && data.name !== pkgData.name) {
          return cb(new Error('Invalid Package: expected ' + pkgData.name +
                              ' but found ' + data.name))
        }

        if (!data.version) {
          return cb(new Error('No version provided'))
        } else if (pkgData.version && data.version !== pkgData.version) {
          return cb(new Error('Invalid Package: expected ' +
                              packageId(pkgData) +
                              ' but found ' + packageId(data)))
        }

        addTmpTarball_(tgz, data, shasum, cb)
      })
    })
  })
}

function addTmpTarball_ (tgz, data, shasum, cb) {
  assert(typeof cb === 'function', 'must have callback function')
  cb = once(cb)

  assert(data.name, 'should have package name by now')
  assert(data.version, 'should have package version by now')

  var root = cachedPackageRoot(data)
  var pkg = path.resolve(root, 'package')
  var target = path.resolve(root, 'package.tgz')
  getCacheStat(function (er, cs) {
    if (er) return cb(er)
    mkdir(pkg, function (er, created) {
      // chown starting from the first dir created by mkdirp,
      // or the root dir, if none had to be created, so that
      // we know that we get all the children.
      function chown () {
        chownr(created || root, cs.uid, cs.gid, done)
      }

      if (er) return cb(er)
      var read = fs.createReadStream(tgz)
      var write = writeStream(target, { mode: npm.modes.file })
      var fin = cs.uid && cs.gid ? chown : done
      read.on('error', cb).pipe(write).on('error', cb).on('close', fin)
    })
  })

  function done () {
    data._shasum = data._shasum || shasum
    cb(null, data)
  }
}
