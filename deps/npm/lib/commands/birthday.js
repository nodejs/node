const BaseCommand = require('../base-command.js')
const log = require('../utils/log-shim')

class Birthday extends BaseCommand {
  static name = 'birthday'
  static description = 'Birthday, deprecated'
  static ignoreImplicitWorkspace = true
  static isShellout = true

  async exec () {
    this.npm.config.set('yes', true)
    log.warn('birthday', 'birthday is deprecated and will be removed in a future release')
    return this.npm.exec('exec', ['@npmcli/npm-birthday'])
  }
}

module.exports = Birthday
