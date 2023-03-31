/* istanbul ignore next */
module.exports = process.env.DEBUG_NOPT || process.env.NOPT_DEBUG
  ? function () {
    console.error.apply(console, arguments)
  }
  : function () {}
