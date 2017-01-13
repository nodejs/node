var crypto = require('crypto')
var fs = require('fs')
var path = require('path')
var chain = require('slide').chain
var log = require('npmlog')
var npm = require('../npm')
var fileCompletion = require('../utils/completion/file-completion.js')

function checksum (str) {
  return crypto
    .createHash('sha1')
    .update(str, 'utf8')
    .digest('hex')
}

function checksumCachedFiles (cb) {
  var tracker = log.newItem('checksumCachedFiles', 1)
  tracker.info('checksumCachedFiles', 'Building file list of ' + npm.cache)
  fileCompletion(npm.cache, '.', Infinity, function (e, files) {
    if (e) {
      tracker.finish()
      return cb(e)
    }
    tracker.addWork(files.length)
    tracker.completeWork(1)
    chain(files.map(andChecksumFile), function (er) {
      tracker.finish()
      cb(null, !er)
    })
    function andChecksumFile (f) {
      return [function (next) { process.nextTick(function () { checksumFile(f, next) }) }]
    }
    function checksumFile (f, next) {
      var file = path.join(npm.cache, f)
      tracker.silly('checksumFile', f)
      if (!/.tgz$/.test(file)) {
        tracker.completeWork(1)
        return next()
      }
      fs.readFile(file, function (err, tgz) {
        tracker.completeWork(1)
        if (err) return next(err)
        try {
          var pkgJSON = fs.readFileSync(path.join(path.dirname(file), 'package/package.json'))
        } catch (e) {
          return next() // no package.json in cche is ok
        }
        try {
          var pkg = JSON.parse(pkgJSON)
          var shasum = (pkg.dist && pkg.dist.shasum) || pkg._shasum
          var actual = checksum(tgz)
          if (actual !== shasum) return next(new Error('Checksum mismatch on ' + file + ', expected: ' + shasum + ', got: ' + shasum))
          return next()
        } catch (e) {
          return next(new Error('Error parsing JSON in ' + file + ': ' + e))
        }
      })
    }
  })
}

module.exports = checksumCachedFiles
