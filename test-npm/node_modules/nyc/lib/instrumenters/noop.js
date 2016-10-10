function NOOP () {
  return {
    instrumentSync: function (code) {
      return code
    },
    lastFileCoverage: function () {
      return null
    }
  }
}

module.exports = NOOP
