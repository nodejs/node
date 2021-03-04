const usageUtil = require('./utils/usage.js')

class Set {
  constructor (npm) {
    this.npm = npm
  }

  get usage () {
    return usageUtil(
      'set',
      'npm set <key>=<value> [<key>=<value> ...] (See `npm config`)'
    )
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  async completion (opts) {
    return this.npm.commands.config.completion(opts)
  }

  exec (args, cb) {
    if (!args.length)
      return cb(this.usage)
    this.npm.commands.config(['set'].concat(args), cb)
  }
}
module.exports = Set
