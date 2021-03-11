const getIdentity = require('./utils/get-identity.js')

const BaseCommand = require('./base-command.js')
class Whoami extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'whoami'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'prints username according to given registry'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[--registry <registry>]']
  }

  exec (args, cb) {
    this.whoami(args).then(() => cb()).catch(cb)
  }

  async whoami (args) {
    const opts = this.npm.flatOptions
    const username = await getIdentity(this.npm, opts)
    this.npm.output(opts.json ? JSON.stringify(username) : username)
  }
}
module.exports = Whoami
