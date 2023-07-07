/* istanbul ignore next */
module.exports = process.env.DEBUG_NOPT || process.env.NOPT_DEBUG
  ? (...a) => console.error(...a)
  : () => {}
