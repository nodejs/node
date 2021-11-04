const envPath = require('../utils/path.js')
const BaseCommand = require('../base-command.js')

class Bin extends BaseCommand {
  static get description () {
    return 'Display npm bin folder'
  }

  static get name () {
    return 'bin'
  }

  static get params () {
    return ['global']
  }

  async exec (args) {
    const b = this.npm.bin
    this.npm.output(b)
    if (this.npm.config.get('global') && !envPath.includes(b))
      console.error('(not in PATH env variable)')
  }
}
module.exports = Bin
