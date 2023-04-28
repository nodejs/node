// The implementation of commands that are just "run a script"
// restart, start, stop, test

const BaseCommand = require('./base-command.js')
class LifecycleCmd extends BaseCommand {
  static usage = ['[-- <args>]']
  static isShellout = true
  static workspaces = true
  static ignoreImplicitWorkspace = false

  async exec (args) {
    return this.npm.exec('run-script', [this.constructor.name, ...args])
  }

  async execWorkspaces (args) {
    return this.npm.exec('run-script', [this.constructor.name, ...args])
  }
}
module.exports = LifecycleCmd
