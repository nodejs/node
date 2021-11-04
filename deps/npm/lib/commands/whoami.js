const getIdentity = require('../utils/get-identity.js')

const BaseCommand = require('../base-command.js')
class Whoami extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Display npm username'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'whoami'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['registry']
  }

  async exec (args) {
    const username = await getIdentity(this.npm, this.npm.flatOptions)
    this.npm.output(
      this.npm.config.get('json') ? JSON.stringify(username) : username
    )
  }
}
module.exports = Whoami
