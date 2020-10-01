const log = require('npmlog')
// output to stdout in a progress bar compatible way
module.exports = (...msg) => {
  log.clearProgress()
  console.log(...msg)
  log.showProgress()
}
