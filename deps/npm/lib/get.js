const BaseCommand = require('./base-command.js')

class Get extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'get'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[<key> ...] (See `npm config`)']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  async completion (opts) {
    return this.npm.commands.config.completion(opts)
  }

  exec (args, cb) {
    this.npm.commands.config(['get'].concat(args), cb)
  }
}
module.exports = Get
