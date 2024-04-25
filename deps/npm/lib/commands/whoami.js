const { output } = require('proc-log')
const getIdentity = require('../utils/get-identity.js')
const BaseCommand = require('../base-cmd.js')

class Whoami extends BaseCommand {
  static description = 'Display npm username'
  static name = 'whoami'
  static params = ['registry']

  async exec (args) {
    const username = await getIdentity(this.npm, { ...this.npm.flatOptions })
    output.standard(
      this.npm.config.get('json') ? JSON.stringify(username) : username
    )
  }
}

module.exports = Whoami
