// npm install-ci-test
// Runs `npm ci` and then runs `npm test`

const CI = require('./ci.js')

class InstallCITest extends CI {
  static get description () {
    return 'Install a project with a clean slate and run tests'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'install-ci-test'
  }

  exec (args, cb) {
    this.npm.commands.ci(args, (er) => {
      if (er)
        return cb(er)
      this.npm.commands.test([], cb)
    })
  }
}
module.exports = InstallCITest
