// The implementation of commands that are just "run a script"
// restart, start, stop, test

const BaseCommand = require('./base-command.js')
class LifecycleCmd extends BaseCommand {
  static get usage () {
    return ['[-- <args>]']
  }

  async exec (args, cb) {
    return this.npm.exec('run-script', [this.constructor.name, ...args])
  }

  async execWorkspaces (args, filters, cb) {
    return this.npm.exec('run-script', [this.constructor.name, ...args])
  }
}
module.exports = LifecycleCmd
