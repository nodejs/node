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

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'ignore-scripts',
      'script-shell',
    ]
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
