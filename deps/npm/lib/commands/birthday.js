const BaseCommand = require('../base-command.js')

class Birthday extends BaseCommand {
  async exec () {
    this.npm.config.set('package', ['@npmcli/npm-birthday'])
    this.npm.config.set('yes', true)
    const exec = await this.npm.cmd('exec')
    return exec.exec(['npm-birthday'])
  }
}

module.exports = Birthday
