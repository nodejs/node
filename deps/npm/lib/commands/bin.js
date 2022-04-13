const log = require('../utils/log-shim.js')
const BaseCommand = require('../base-command.js')
// TODO this may not be needed at all. Ignoring because our tests normalize
// process.env.path already
/* istanbul ignore next */
const path = process.env.path || process.env.Path || process.env.PATH
const { delimiter } = require('path')

class Bin extends BaseCommand {
  static description = 'Display npm bin folder'
  static name = 'bin'
  static params = ['global']
  static ignoreImplicitWorkspace = true

  async exec (args) {
    const b = this.npm.bin
    this.npm.output(b)
    if (this.npm.config.get('global') && !path.split(delimiter).includes(b)) {
      log.error('bin', '(not in PATH env variable)')
    }
  }
}
module.exports = Bin
