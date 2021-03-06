// The implementation of commands that are just "run a script"
// restart, start, stop, test
const usageUtil = require('./usage.js')

class LifecycleCmd {
  constructor (npm, stage) {
    this.npm = npm
    this.stage = stage
  }

  get usage () {
    return usageUtil(this.stage, `npm ${this.stage} [-- <args>]`)
  }

  exec (args, cb) {
    this.npm.commands['run-script']([this.stage, ...args], cb)
  }
}
module.exports = LifecycleCmd
