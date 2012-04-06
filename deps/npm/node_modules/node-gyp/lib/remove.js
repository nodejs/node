
module.exports = exports = remove

exports.usage = 'Removes the node development files for the specified version'

/**
 * Module dependencies.
 */

var fs = require('fs')
  , rm = require('rimraf')
  , path = require('path')
  , semver = require('semver')

function remove (gyp, argv, callback) {

  // TODO: Make ~/.node-gyp configurable
  var nodeGypDir = path.resolve(process.env.HOME, '.node-gyp')

  gyp.verbose('using node-gyp dir', nodeGypDir)

  // get the user-specified version to remove
  var v = argv[0] || gyp.opts.target

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

  var versionPath = path.resolve(nodeGypDir, version)
  gyp.verbose('removing development files for version', version)

  // first check if its even installed
  fs.stat(versionPath, function (err, stat) {
    if (err) {
      if (err.code == 'ENOENT') {
        gyp.info('version was already not installed', version)
        callback()
      } else {
        callback(err)
      }
      return
    }
    // Go ahead and delete the dir
    rm(versionPath, callback)
  })

}
