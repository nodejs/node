// npm install-test
// Runs `npm install` and then runs `npm test`

const usageUtil = require('./utils/usage.js')

class InstallTest {
  constructor (npm) {
    this.npm = npm
  }

  get usage () {
    return usageUtil(
      'install-test',
      'npm install-test [args]' +
      '\nSame args as `npm install`'
    )
  }

  async completion (opts) {
    return this.npm.commands.install.completion(opts)
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
