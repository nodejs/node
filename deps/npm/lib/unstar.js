const Star = require('./star.js')

class Unstar extends Star {
  exec (args, cb) {
    this.npm.config.set('star.unstar', true)
    super.exec(args, cb)
  }
}
module.exports = Unstar
