const BaseCommand = require('../../base-cmd.js')

class Trust extends BaseCommand {
  static description = 'Create a trusted relationship between a package and a OIDC provider'
  static name = 'trust'

  static subcommands = {
    github: require('./github.js'),
    gitlab: require('./gitlab.js'),
    list: require('./list.js'),
    revoke: require('./revoke.js'),
  }

  static async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv.length === 2) {
      return Object.keys(Trust.subcommands)
    }
    return []
  }
}

module.exports = Trust
