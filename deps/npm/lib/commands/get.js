const BaseCommand = require('../base-command.js')

class Get extends BaseCommand {
  static description = 'Get a value from the npm configuration'
  static name = 'get'
  static usage = ['[<key> ...] (See `npm config`)']

  // TODO
  /* istanbul ignore next */
  async completion (opts) {
    const config = await this.npm.cmd('config')
    return config.completion(opts)
  }

  async exec (args) {
    return this.npm.exec('config', ['get'].concat(args))
  }
}
module.exports = Get
