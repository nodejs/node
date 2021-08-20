const log = require('npmlog')
const { inspect } = require('util')
module.exports = () => {
  process.on('log', (level, ...args) => {
    try {
      log[level](...args)
    } catch (ex) {
      try {
        // if it crashed once, it might again!
        log.verbose(`attempt to log ${inspect([level, ...args])} crashed`, ex)
      } catch (ex2) {
        console.error(`attempt to log ${inspect([level, ...args])} crashed`, ex)
      }
    }
  })
}

// for tests
/* istanbul ignore next */
module.exports.reset = () => {
  process.removeAllListeners('log')
}
