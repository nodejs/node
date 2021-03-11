// The implementation of commands that are just "run a script"
// restart, start, stop, test

const BaseCommand = require('../base-command.js')
class LifecycleCmd extends BaseCommand {
  static get usage () {
    return ['[-- <args>]']
  }

  exec (args, cb) {
    this.npm.commands['run-script']([this.constructor.name, ...args], cb)
  }
}
module.exports = LifecycleCmd
