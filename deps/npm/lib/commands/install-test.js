// npm install-test
// Runs `npm install` and then runs `npm test`

const Install = require('./install.js')

class InstallTest extends Install {
  static get description () {
    return 'Install package(s) and run tests'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'install-test'
  }

  async exec (args, cb) {
    await this.npm.exec('install', args)
    return this.npm.exec('test', [])
  }
}
module.exports = InstallTest
