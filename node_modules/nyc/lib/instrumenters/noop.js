var FileCoverage = require('istanbul-lib-coverage').classes.FileCoverage
var readInitialCoverage = require('istanbul-lib-instrument').readInitialCoverage

function NOOP () {
  return {
    instrumentSync: function (code, filename) {
      var extracted = readInitialCoverage(code)
      if (extracted) {
        this.fileCoverage = new FileCoverage(extracted.coverageData)
      } else {
        this.fileCoverage = null
      }
      return code
    },
    lastFileCoverage: function () {
      return this.fileCoverage
    }
  }
}

module.exports = NOOP
