const BaseCommand = require('../base-command.js')

class Prefix extends BaseCommand {
  static description = 'Display prefix'
  static name = 'prefix'
  static params = ['global']
  static usage = ['[-g]']

  async exec (args) {
    return this.npm.output(this.npm.prefix)
  }
}
module.exports = Prefix
