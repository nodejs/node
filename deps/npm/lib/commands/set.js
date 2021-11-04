const BaseCommand = require('../base-command.js')

class Set extends BaseCommand {
  static get description () {
    return 'Set a value in the npm configuration'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'set'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['<key>=<value> [<key>=<value> ...] (See `npm config`)']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  async completion (opts) {
    return this.npm.cmd('config').completion(opts)
  }

  async exec (args) {
    if (!args.length)
      throw this.usageError()
    return this.npm.exec('config', ['set'].concat(args))
  }
}
module.exports = Set
