const BaseCommand = require('../base-command.js')

class Set extends BaseCommand {
  static description = 'Set a value in the npm configuration'
  static name = 'set'
  static usage = ['<key>=<value> [<key>=<value> ...] (See `npm config`)']

  // TODO
  /* istanbul ignore next */
  async completion (opts) {
    return this.npm.cmd('config').completion(opts)
  }

  async exec (args) {
    if (!args.length) {
      throw this.usageError()
    }
    return this.npm.exec('config', ['set'].concat(args))
  }
}
module.exports = Set
