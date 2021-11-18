const envPath = require('../utils/path.js')
const BaseCommand = require('../base-command.js')

class Bin extends BaseCommand {
  static description = 'Display npm bin folder'
  static name = 'bin'
  static params = ['global']

  async exec (args) {
    const b = this.npm.bin
    this.npm.output(b)
    if (this.npm.config.get('global') && !envPath.includes(b)) {
      console.error('(not in PATH env variable)')
    }
  }
}
module.exports = Bin
