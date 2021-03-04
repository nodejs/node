const LS = require('./ls.js')
const usageUtil = require('./utils/usage.js')

class LL extends LS {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil(
      'll',
      'npm ll [[<@scope>/]<pkg> ...]'
    )
  }

  exec (args, cb) {
    this.npm.config.set('long', true)
    super.exec(args, cb)
  }
}

module.exports = LL
