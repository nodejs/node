const getIdentity = require('../utils/get-identity.js')
const log = require('../utils/log-shim.js')

const BaseCommand = require('../base-command.js')
class Whoami extends BaseCommand {
  static description = 'Display npm username'
  static name = 'whoami'
  static params = ['registry']

  async exec (args) {
    const username = await getIdentity(this.npm, { ...this.npm.flatOptions, log })
    this.npm.output(
      this.npm.config.get('json') ? JSON.stringify(username) : username
    )
  }
}
module.exports = Whoami
