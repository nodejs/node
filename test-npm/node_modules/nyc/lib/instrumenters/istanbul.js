function InstrumenterIstanbul (cwd) {
  var istanbul = InstrumenterIstanbul.istanbul()

  return istanbul.createInstrumenter({
    autoWrap: true,
    coverageVariable: '__coverage__',
    embedSource: true,
    noCompact: false,
    preserveComments: true
  })
}

InstrumenterIstanbul.istanbul = function () {
  InstrumenterIstanbul._istanbul || (InstrumenterIstanbul._istanbul = require('istanbul-lib-instrument'))

  return InstrumenterIstanbul._istanbul || (InstrumenterIstanbul._istanbul = require('istanbul'))
}

module.exports = InstrumenterIstanbul
