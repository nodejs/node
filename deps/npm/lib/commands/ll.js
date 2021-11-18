const LS = require('./ls.js')

class LL extends LS {
  static name = 'll'
  static usage = ['[[<@scope>/]<pkg> ...]']

  async exec (args) {
    this.npm.config.set('long', true)
    return super.exec(args)
  }
}

module.exports = LL
