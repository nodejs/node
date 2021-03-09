// npm install-test
// Runs `npm install` and then runs `npm test`

const Install = require('./install.js')

class InstallTest extends Install {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'install-test'
  }

  exec (args, cb) {
    this.npm.commands.install(args, (er) => {
      if (er)
        return cb(er)
      this.npm.commands.test([], cb)
    })
  }
}
module.exports = InstallTest
