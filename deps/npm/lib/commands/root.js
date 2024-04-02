const BaseCommand = require('../base-command.js')
class Root extends BaseCommand {
  static description = 'Display npm root'
  static name = 'root'
  static params = ['global']

  async exec () {
    this.npm.output(this.npm.dir)
  }
}
module.exports = Root
