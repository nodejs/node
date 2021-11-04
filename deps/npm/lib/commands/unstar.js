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

  async exec (args) {
    this.npm.config.set('star.unstar', true)
    return super.exec(args)
  }
}
module.exports = Unstar
