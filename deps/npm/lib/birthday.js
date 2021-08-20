const BaseCommand = require('./base-command.js')

class Birthday extends BaseCommand {
  exec (args, cb) {
    this.npm.config.set('package', ['@npmcli/npm-birthday'])
    this.npm.config.set('yes', true)
    return this.npm.commands.exec(['npm-birthday'], cb)
  }
}

module.exports = Birthday
