const Star = require('./star.js')

class Unstar extends Star {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Remove an item from your favorite packages'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'unstar'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'registry',
      'unicode',
      'otp',
    ]
  }

  exec (args, cb) {
    this.npm.config.set('star.unstar', true)
    super.exec(args, cb)
  }
}
module.exports = Unstar
