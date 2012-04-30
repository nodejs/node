
module.exports = exports = clean

exports.usage = 'Removes any generated build files and the "out" dir'

/**
 * Module dependencies.
 */

var rm = require('rimraf')


function clean (gyp, argv, callback) {

  // Remove the 'build' dir
  var buildDir = 'build'

  gyp.verbose('removing "build" directory')
  rm(buildDir, callback)

}
