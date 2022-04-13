const Star = require('./star.js')

class Unstar extends Star {
  static description = 'Remove an item from your favorite packages'
  static name = 'unstar'
  static params = [
    'registry',
    'unicode',
    'otp',
  ]

  async exec (args) {
    this.npm.config.set('star.unstar', true)
    return super.exec(args)
  }
}
module.exports = Unstar
