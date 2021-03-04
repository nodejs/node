const output = require('./utils/output.js')
const getIdentity = require('./utils/get-identity.js')
const usageUtil = require('./utils/usage.js')

class Whoami {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil(
      'whoami',
      'npm whoami [--registry <registry>]\n' +
      '(just prints username according to given registry)'
    )
  }

  exec (args, cb) {
    this.whoami(args).then(() => cb()).catch(cb)
  }

  async whoami (args) {
    const opts = this.npm.flatOptions
    const username = await getIdentity(this.npm, opts)
    output(opts.json ? JSON.stringify(username) : username)
  }
}
module.exports = Whoami
