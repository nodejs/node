
module.exports = exports = clean

exports.usage = 'Removes any generated build files and the "out" dir'

/**
 * Module dependencies.
 */

var rm = require('rimraf')
  , asyncEmit = require('./util/asyncEmit')
  , createHook = require('./util/hook')


function clean (gyp, argv, callback) {

  // Remove the 'build' dir
  var buildDir = 'build'
    , emitter

  createHook('gyp-clean.js', function (err, _e) {
    if (err) return callback(err)
    emitter = _e
    asyncEmit(emitter, 'before', function (err) {
      if (err) return callback(err)
      doClean()
    })
  })

  function doClean () {
    gyp.verbose('removing "build" directory')
    rm(buildDir, after)
  }

  function after () {
    asyncEmit(emitter, 'after', function (err) {
      if (err) return callback(err)
      callback()
    })
  }

}
