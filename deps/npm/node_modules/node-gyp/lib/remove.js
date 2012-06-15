
module.exports = exports = remove

exports.usage = 'Removes the node development files for the specified version'

/**
 * Module dependencies.
 */

var fs = require('fs')
  , rm = require('rimraf')
  , path = require('path')
  , log = require('npmlog')
  , semver = require('semver')

function remove (gyp, argv, callback) {

  var devDir = gyp.devDir
  log.verbose('remove', 'using node-gyp dir:', devDir)

  // get the user-specified version to remove
  var v = argv[0] || gyp.opts.target
  log.verbose('remove', 'removing target version:', v)

  if (!v) {
    return callback(new Error('You must specify a version number to remove. Ex: "' + process.version + '"'))
  }

  // parse the version to normalize and make sure it's valid
  var version = semver.parse(v)

  if (!version) {
    return callback(new Error('Invalid version number: ' + v))
  }

  // flatten the version Array into a String
  version = version.slice(1, 4).join('.')

  var versionPath = path.resolve(gyp.devDir, version)
  log.verbose('remove', 'removing development files for version:', version)

  // first check if its even installed
  fs.stat(versionPath, function (err, stat) {
    if (err) {
      if (err.code == 'ENOENT') {
        callback(null, 'version was already uninstalled: ' + version)
      } else {
        callback(err)
      }
      return
    }
    // Go ahead and delete the dir
    rm(versionPath, callback)
  })

}
