const Star = require('./star.js')

class Unstar extends Star {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'unstar'
  }

  exec (args, cb) {
    this.npm.config.set('star.unstar', true)
    super.exec(args, cb)
  }
}
module.exports = Unstar
