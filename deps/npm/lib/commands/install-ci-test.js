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

  async exec (args, cb) {
    await this.npm.exec('ci', args)
    return this.npm.exec('test', [])
  }
}
module.exports = InstallCITest
