'use strict'
var path = require('path')
var caller = require('caller')

exports = module.exports = function (toLoad, mocks) {
  return requireInject(toLoad, mocks)
}

exports.withEmptyCache = function (toLoad, mocks) {
  return requireInject(toLoad, mocks, true)
}

exports.installGlobally = installGlobally

exports.installGlobally.andClearCache = function (toLoad, mocks) {
  var callerFilename = getCallerFilename()
  Object.keys(require.cache).forEach(function (name) {
    if (name !== callerFilename) delete require.cache[name]
  })
  return installGlobally(toLoad, mocks)
}

var requireInject = function (toLoad, mocks, withEmptyCache) {
  // Copy the existing cache
  var originalCache = {}
  Object.keys(require.cache).forEach(function (name) {
    originalCache[name] = require.cache[name]
  })

  var mocked = withEmptyCache
    ? installGlobally.andClearCache(toLoad, mocks)
    : installGlobally(toLoad, mocks)

  // restore the cache, we can't just assign originalCache to require.cache as the require
  // object is unique to each module, even though require.cache is shared
  Object.keys(require.cache).forEach(function (name) { delete require.cache[name] })
  Object.keys(originalCache).forEach(function (name) { require.cache[name] = originalCache[name] })

  return mocked
}

function resolve (callerFilename, name) {
  if (/^[.][.]?\//.test(name)) {
    name = path.resolve(path.dirname(callerFilename), name)
  }
  return require.resolve(name)
}

function getCallerFilename () {
  var callerFilename
  for (var ii = 1; ii <= 10; ++ii) {
    callerFilename = caller(ii)
    if (callerFilename !== module.filename) return callerFilename
  }
  throw new Error("Couldn't find caller that wasn't " + module.filename + ' in most recent 10 stackframes')
}

function installGlobally (toLoad, mocks) {
  var callerFilename = getCallerFilename()

  // Inject all of our mocks
  Object.keys(mocks).forEach(function (name) {
    var namePath = resolve(callerFilename, name)
    if (mocks[name] == null) {
      delete require.cache[namePath]
    } else {
      require.cache[namePath] = {exports: mocks[name]}
    }
  })

  var toLoadPath = resolve(callerFilename, toLoad)

  // remove any unmocked version previously loaded
  delete require.cache[toLoadPath]
  // load our new version using our mocks
  return require.cache[callerFilename].require(toLoadPath)
}
