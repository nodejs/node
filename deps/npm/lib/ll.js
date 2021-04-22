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

  exec (args, cb) {
    this.npm.config.set('long', true)
    super.exec(args, cb)
  }
}

module.exports = LL
