'use strict'

var convertSourceMap = require('convert-source-map')
var mergeSourceMap = require('merge-source-map')

function InstrumenterIstanbul (cwd, options) {
  var istanbul = InstrumenterIstanbul.istanbul()
  var instrumenter = istanbul.createInstrumenter({
    autoWrap: true,
    coverageVariable: '__coverage__',
    embedSource: true,
    noCompact: false,
    preserveComments: true,
    produceSourceMap: options.produceSourceMap,
    esModules: true
  })

  return {
    instrumentSync: function (code, filename, sourceMap) {
      var instrumented = instrumenter.instrumentSync(code, filename)
      // the instrumenter can optionally produce source maps,
      // this is useful for features like remapping stack-traces.
      // TODO: test source-map merging logic.
      if (options.produceSourceMap) {
        var lastSourceMap = instrumenter.lastSourceMap()
        if (lastSourceMap) {
          if (sourceMap) {
            lastSourceMap = mergeSourceMap(
              sourceMap.toObject(),
              lastSourceMap
            )
          }
          instrumented += '\n' + convertSourceMap.fromObject(lastSourceMap).toComment()
        }
      }
      return instrumented
    },
    lastFileCoverage: function () {
      return instrumenter.lastFileCoverage()
    }
  }
}

InstrumenterIstanbul.istanbul = function () {
  InstrumenterIstanbul._istanbul || (InstrumenterIstanbul._istanbul = require('istanbul-lib-instrument'))

  return InstrumenterIstanbul._istanbul || (InstrumenterIstanbul._istanbul = require('istanbul'))
}

module.exports = InstrumenterIstanbul
