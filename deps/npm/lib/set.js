const BaseCommand = require('./base-command.js')

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
    return this.npm.commands.config.completion(opts)
  }

  exec (args, cb) {
    if (!args.length)
      return cb(this.usageError())
    this.npm.commands.config(['set'].concat(args), cb)
  }
}
module.exports = Set
