const LS = require('./ls.js')

class LL extends LS {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'll'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[[<@scope>/]<pkg> ...]']
  }

  async exec (args) {
    this.npm.config.set('long', true)
    return super.exec(args)
  }
}

module.exports = LL
