'use strict'
var fs = require('graceful-fs')
var path = require('path')
var zlib = require('zlib')

var log = require('npmlog')
var realizePackageSpecifier = require('realize-package-specifier')
var tar = require('tar')
var once = require('once')
var semver = require('semver')
var readPackageTree = require('read-package-tree')
var readPackageJson = require('read-package-json')
var iferr = require('iferr')
var rimraf = require('rimraf')
var clone = require('lodash.clonedeep')
var validate = require('aproba')
var unpipe = require('unpipe')
var normalizePackageData = require('normalize-package-data')

var npm = require('./npm.js')
var mapToRegistry = require('./utils/map-to-registry.js')
var cache = require('./cache.js')
var cachedPackageRoot = require('./cache/cached-package-root.js')
var tempFilename = require('./utils/temp-filename.js')
var getCacheStat = require('./cache/get-stat.js')
var unpack = require('./utils/tar.js').unpack
var pulseTillDone = require('./utils/pulse-till-done.js')
var parseJSON = require('./utils/parse-json.js')

function andLogAndFinish (spec, tracker, done) {
  validate('SF', [spec, done])
  return function (er, pkg) {
    if (er) {
      log.silly('fetchPackageMetaData', 'error for ' + spec, er)
      if (tracker) tracker.finish()
    }
    return done(er, pkg)
  }
}

module.exports = function fetchPackageMetadata (spec, where, tracker, done) {
  if (!done) {
    done = tracker || where
    tracker = null
    if (done === where) where = null
  }
  if (typeof spec === 'object') {
    var dep = spec
    spec = dep.raw
  }
  var logAndFinish = andLogAndFinish(spec, tracker, done)
  if (!dep) {
    log.silly('fetchPackageMetaData', spec)
    return realizePackageSpecifier(spec, where, iferr(logAndFinish, function (dep) {
      fetchPackageMetadata(dep, where, tracker, done)
    }))
  }
  if (dep.type === 'version' || dep.type === 'range' || dep.type === 'tag') {
    fetchNamedPackageData(dep, addRequestedAndFinish)
  } else if (dep.type === 'directory') {
    fetchDirectoryPackageData(dep, where, addRequestedAndFinish)
  } else {
    fetchOtherPackageData(spec, dep, where, addRequestedAndFinish)
  }
  function addRequestedAndFinish (er, pkg) {
    if (pkg) annotateMetadata(pkg, dep, spec, where)
    logAndFinish(er, pkg)
  }
}

var annotateMetadata = module.exports.annotateMetadata = function (pkg, requested, spec, where) {
  validate('OOSS', arguments)
  pkg._requested = requested
  pkg._spec = spec
  pkg._where = where
  if (!pkg._args) pkg._args = []
  pkg._args.push([requested, where])
  // non-npm registries can and will return unnormalized data, plus
  // even the npm registry may have package data normalized with older
  // normalization rules. This ensures we get package data in a consistent,
  // stable format.
  try {
    normalizePackageData(pkg)
  } catch (ex) {
    // don't care
  }
}

function fetchOtherPackageData (spec, dep, where, next) {
  validate('SOSF', arguments)
  log.silly('fetchOtherPackageData', spec)
  cache.add(spec, null, where, false, iferr(next, function (pkg) {
    var result = clone(pkg)
    result._inCache = true
    next(null, result)
  }))
}

function fetchDirectoryPackageData (dep, where, next) {
  validate('OSF', arguments)
  log.silly('fetchDirectoryPackageData', dep.name || dep.rawSpec)
  readPackageJson(path.join(dep.spec, 'package.json'), false, next)
}

var regCache = {}

function fetchNamedPackageData (dep, next) {
  validate('OF', arguments)
  log.silly('fetchNamedPackageData', dep.name || dep.rawSpec)
  mapToRegistry(dep.name || dep.rawSpec, npm.config, iferr(next, function (url, auth) {
    if (regCache[url]) {
      pickVersionFromRegistryDocument(clone(regCache[url]))
    } else {
      npm.registry.get(url, {auth: auth}, pulseTillDone('fetchMetadata', iferr(next, pickVersionFromRegistryDocument)))
    }
    function returnAndAddMetadata (pkg) {
      pkg._from = dep.raw
      pkg._resolved = pkg.dist.tarball
      pkg._shasum = pkg.dist.shasum

      next(null, pkg)
    }
    function pickVersionFromRegistryDocument (pkg) {
      if (!regCache[url]) regCache[url] = pkg
      var versions = Object.keys(pkg.versions)

      var invalidVersions = versions.filter(function (v) { return !semver.valid(v) })
      if (invalidVersions.length > 0) {
        log.warn('pickVersion', 'The package %s has invalid semver-version(s): %s. This usually only happens for unofficial private registries. ' +
            'You should delete or re-publish the invalid versions.', pkg.name, invalidVersions.join(', '))
      }

      versions = versions.filter(function (v) { return semver.valid(v) }).sort(semver.rcompare)

      if (dep.type === 'tag') {
        var tagVersion = pkg['dist-tags'][dep.spec]
        if (pkg.versions[tagVersion]) return returnAndAddMetadata(pkg.versions[tagVersion])
      } else {
        var latestVersion = pkg['dist-tags'][npm.config.get('tag')] || versions[0]

        // Find the the most recent version less than or equal
        // to latestVersion that satisfies our spec
        for (var ii = 0; ii < versions.length; ++ii) {
          if (semver.gt(versions[ii], latestVersion)) continue
          if (semver.satisfies(versions[ii], dep.spec)) {
            return returnAndAddMetadata(pkg.versions[versions[ii]])
          }
        }

        // Failing that, try finding the most recent version that matches
        // our spec
        for (var jj = 0; jj < versions.length; ++jj) {
          if (semver.satisfies(versions[jj], dep.spec)) {
            return returnAndAddMetadata(pkg.versions[versions[jj]])
          }
        }

        // Failing THAT, if the range was '*' uses latestVersion
        if (dep.spec === '*') {
          return returnAndAddMetadata(pkg.versions[latestVersion])
        }
      }

      // We didn't manage to find a compatible version
      // If this package was requested from cache, force hitting the network
      if (pkg._cached) {
        log.silly('fetchNamedPackageData', 'No valid target from cache, forcing network')
        return npm.registry.get(url, {
          auth: auth,
          skipCache: true
        }, pulseTillDone('fetchMetadata', iferr(next, pickVersionFromRegistryDocument)))
      }

      // And failing that, we error out
      var targets = versions.length
                  ? 'Valid install targets:\n' + versions.join(', ') + '\n'
                  : 'No valid targets found.'
      var er = new Error('No compatible version found: ' +
                         dep.raw + '\n' + targets)
      er.code = 'ETARGET'
      return next(er)
    }
  }))
}

function retryWithCached (pkg, asserter, next) {
  if (!pkg._inCache) {
    cache.add(pkg._spec, null, pkg._where, false, iferr(next, function (newpkg) {
      Object.keys(newpkg).forEach(function (key) {
        if (key[0] !== '_') return
        pkg[key] = newpkg[key]
      })
      pkg._inCache = true
      return asserter(pkg, next)
    }))
  }
  return !pkg._inCache
}

module.exports.addShrinkwrap = function addShrinkwrap (pkg, next) {
  validate('OF', arguments)
  if (pkg._shrinkwrap !== undefined) return next(null, pkg)
  if (retryWithCached(pkg, addShrinkwrap, next)) return
  pkg._shrinkwrap = null
  // FIXME: cache the shrinkwrap directly
  var pkgname = pkg.name
  var ver = pkg.version
  var tarball = path.join(cachedPackageRoot({name: pkgname, version: ver}), 'package.tgz')
  untarStream(tarball, function (er, untar) {
    if (er) {
      if (er.code === 'ENOTTARBALL') {
        pkg._shrinkwrap = null
        return next()
      } else {
        return next(er)
      }
    }
    if (er) return next(er)
    var foundShrinkwrap = false
    untar.on('entry', function (entry) {
      if (!/^(?:[^\/]+[\/])npm-shrinkwrap.json$/.test(entry.path)) return
      log.silly('addShrinkwrap', 'Found shrinkwrap in ' + pkgname + ' ' + entry.path)
      foundShrinkwrap = true
      var shrinkwrap = ''
      entry.on('data', function (chunk) {
        shrinkwrap += chunk
      })
      entry.on('end', function () {
        untar.close()
        log.silly('addShrinkwrap', 'Completed reading shrinkwrap in ' + pkgname)
        try {
          pkg._shrinkwrap = parseJSON(shrinkwrap)
        } catch (ex) {
          var er = new Error('Error parsing ' + pkgname + '@' + ver + "'s npm-shrinkwrap.json: " + ex.message)
          er.type = 'ESHRINKWRAP'
          return next(er)
        }
        next(null, pkg)
      })
      entry.resume()
    })
    untar.on('end', function () {
      if (!foundShrinkwrap) {
        pkg._shrinkwrap = null
        next(null, pkg)
      }
    })
  })
}

module.exports.addBundled = function addBundled (pkg, next) {
  validate('OF', arguments)
  if (pkg._bundled !== undefined) return next(null, pkg)
  if (!pkg.bundleDependencies) return next(null, pkg)
  if (retryWithCached(pkg, addBundled, next)) return
  pkg._bundled = null
  var pkgname = pkg.name
  var ver = pkg.version
  var tarball = path.join(cachedPackageRoot({name: pkgname, version: ver}), 'package.tgz')
  var target = tempFilename('unpack')
  getCacheStat(iferr(next, function (cs) {
    log.verbose('addBundled', 'extract', tarball)
    unpack(tarball, target, null, null, cs.uid, cs.gid, iferr(next, function () {
      log.silly('addBundled', 'read tarball')
      readPackageTree(target, function (er, tree) {
        log.silly('cleanup', 'remove extracted module')
        rimraf(target, function () {
          if (tree) {
            pkg._bundled = tree.children
          }
          next(null, pkg)
        })
      })
    }))
  }))
}

// FIXME: hasGzipHeader / hasTarHeader / untarStream duplicate a lot
// of code from lib/utils/tar.js– these should be brought together.

function hasGzipHeader (c) {
  return c[0] === 0x1F && c[1] === 0x8B && c[2] === 0x08
}

function hasTarHeader (c) {
  return c[257] === 0x75 && // tar archives have 7573746172 at position
         c[258] === 0x73 && // 257 and 003030 or 202000 at position 262
         c[259] === 0x74 &&
         c[260] === 0x61 &&
         c[261] === 0x72 &&

       ((c[262] === 0x00 &&
         c[263] === 0x30 &&
         c[264] === 0x30) ||

        (c[262] === 0x20 &&
         c[263] === 0x20 &&
         c[264] === 0x00))
}

function untarStream (tarball, cb) {
  validate('SF', arguments)
  cb = once(cb)

  var stream
  var file = stream = fs.createReadStream(tarball)
  var tounpipe = [file]
  file.on('error', function (er) {
    er = new Error('Error extracting ' + tarball + ' archive: ' + er.message)
    er.code = 'EREADFILE'
    cb(er)
  })
  file.on('data', function OD (c) {
    if (hasGzipHeader(c)) {
      doGunzip()
    } else if (hasTarHeader(c)) {
      doUntar()
    } else {
      if (file.close) file.close()
      if (file.destroy) file.destroy()
      var er = new Error('Non-gzip/tarball ' + tarball)
      er.code = 'ENOTTARBALL'
      return cb(er)
    }
    file.removeListener('data', OD)
    file.emit('data', c)
    cb(null, stream)
  })

  function doGunzip () {
    var gunzip = stream.pipe(zlib.createGunzip())
    gunzip.on('error', function (er) {
      er = new Error('Error extracting ' + tarball + ' archive: ' + er.message)
      er.code = 'EGUNZIP'
      cb(er)
    })
    tounpipe.push(gunzip)
    stream = gunzip
    doUntar()
  }

  function doUntar () {
    var untar = stream.pipe(tar.Parse())
    untar.on('error', function (er) {
      er = new Error('Error extracting ' + tarball + ' archive: ' + er.message)
      er.code = 'EUNTAR'
      cb(er)
    })
    tounpipe.push(untar)
    stream = untar
    addClose()
  }

  function addClose () {
    stream.close = function () {
      tounpipe.forEach(function (stream) {
        unpipe(stream)
      })

      if (file.close) file.close()
      if (file.destroy) file.destroy()
    }
  }
}
