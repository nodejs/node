// npm install-ci-test
// Runs `npm ci` and then runs `npm test`

const usageUtil = require('./utils/usage.js')

class InstallCITest {
  constructor (npm) {
    this.npm = npm
  }

  get usage () {
    return usageUtil(
      'install-ci-test',
      'npm install-ci-test [args]' +
      '\nSame args as `npm ci`'
    )
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
