const convertSourceMap = require('convert-source-map')
const libCoverage = require('istanbul-lib-coverage')
const libSourceMaps = require('istanbul-lib-source-maps')
const fs = require('fs')
const path = require('path')

// TODO: write some unit tests for this class.
function SourceMaps (opts) {
  this.cache = opts.cache
  this.cacheDirectory = opts.cacheDirectory
  this.sourceMapCache = libSourceMaps.createSourceMapStore()
  this.loadedMaps = {}
}

SourceMaps.prototype.extractAndRegister = function (code, filename, hash) {
  var sourceMap = convertSourceMap.fromSource(code) || convertSourceMap.fromMapFileSource(code, path.dirname(filename))
  if (sourceMap) {
    if (this.cache && hash) {
      var mapPath = path.join(this.cacheDirectory, hash + '.map')
      fs.writeFileSync(mapPath, sourceMap.toJSON())
    } else {
      this.sourceMapCache.registerMap(filename, sourceMap.sourcemap)
    }
  }
  return sourceMap
}

SourceMaps.prototype.remapCoverage = function (obj) {
  var transformed = this.sourceMapCache.transformCoverage(
    libCoverage.createCoverageMap(obj)
  )
  return transformed.map.data
}

SourceMaps.prototype.reloadCachedSourceMaps = function (report) {
  var _this = this
  Object.keys(report).forEach(function (absFile) {
    var fileReport = report[absFile]
    if (fileReport && fileReport.contentHash) {
      var hash = fileReport.contentHash
      if (!(hash in _this.loadedMaps)) {
        try {
          var mapPath = path.join(_this.cacheDirectory, hash + '.map')
          _this.loadedMaps[hash] = JSON.parse(fs.readFileSync(mapPath, 'utf8'))
        } catch (e) {
          // set to false to avoid repeatedly trying to load the map
          _this.loadedMaps[hash] = false
        }
      }
      if (_this.loadedMaps[hash]) {
        _this.sourceMapCache.registerMap(absFile, _this.loadedMaps[hash])
      }
    }
  })
}

module.exports = SourceMaps
