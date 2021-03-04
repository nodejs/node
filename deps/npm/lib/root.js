const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')

class Root {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil('root', 'npm root [-g]')
  }

  exec (args, cb) {
    this.root(args).then(() => cb()).catch(cb)
  }

  async root () {
    output(this.npm.dir)
  }
}
module.exports = Root
