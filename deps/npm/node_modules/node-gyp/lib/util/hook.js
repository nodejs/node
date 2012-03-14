
/**
 * This is our "Hook" class that allows a script to hook into the lifecyle of the
 * "configure", "build" and "clean" commands. It's basically a hack into the
 * module.js file to allow custom hooks into the module-space, specifically to
 * make the global scope act as an EventEmitter.
 */

var fs = require('fs')
  , path = require('path')
  , Module = require('module')
  , EventEmitter = require('events').EventEmitter
  , functions = Object.keys(EventEmitter.prototype).filter(function (k) {
      return typeof EventEmitter.prototype[k] == 'function'
    })
  , boilerplate = functions.map(function (k) {
      return 'var ' + k + ' = module.emitter.' + k + '.bind(module.emitter);'
    }).join('')

module.exports = createHook
function createHook (filename, callback) {

  var emitter = new EventEmitter

  // first read the file contents
  fs.readFile(filename, 'utf8', function (err, code) {
    if (err) {
      if (err.code == 'ENOENT') {
        // hook file doesn't exist, oh well
        callback(null, emitter)
      } else {
        callback(err)
      }
      return
    }
    // got a hook file, now execute it
    var mod = new Module(filename)
    mod.filename = filename
    mod.paths = Module._nodeModulePaths(filename)
    mod.emitter = emitter
    try {
      mod._compile(boilerplate + code, filename)
    } catch (e) {
      return callback(e)
    }
    callback(null, emitter)
  })
}
