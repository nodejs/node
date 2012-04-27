
module.exports = exports = list

exports.usage = 'Prints a listing of the currently installed node development files'

/**
 * Module dependencies.
 */

var fs = require('graceful-fs')
  , path = require('path')

function list (gyp, args, callback) {

  gyp.verbose('using node-gyp dir', gyp.devDir)

  // readdir the node-gyp dir
  fs.readdir(gyp.devDir, onreaddir)

  function onreaddir (err, versions) {
    if (err && err.code != 'ENOENT') {
      return callback(err)
    }
    if (versions) {
      versions = versions.filter(function (v) { return v != 'current' })
    } else {
      versions = []
    }
    callback(null, versions)
  }
}
