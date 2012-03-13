
module.exports = exports = list

exports.usage = 'Prints a listing of the currently installed node development files'

/**
 * Module dependencies.
 */

var fs = require('fs')
  , path = require('path')

function list (gyp, args, callback) {

  // TODO: Make ~/.node-gyp configurable
  var nodeGypDir = path.resolve(process.env.HOME, '.node-gyp')

  gyp.verbose('using node-gyp dir', nodeGypDir)

  // readdir the node-gyp dir
  fs.readdir(nodeGypDir, onreaddir)

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
