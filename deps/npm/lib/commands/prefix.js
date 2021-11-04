const BaseCommand = require('../base-command.js')

class Prefix extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Display prefix'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'prefix'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['global']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[-g]']
  }

  async exec (args) {
    return this.npm.output(this.npm.prefix)
  }
}
module.exports = Prefix
