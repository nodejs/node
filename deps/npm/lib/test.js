const LifecycleCmd = require('./utils/lifecycle-cmd.js')

// This ends up calling run-script(['test', ...args])
class Test extends LifecycleCmd {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Test a package'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'test'
  }

  exec (args, cb) {
    super.exec(args, er => {
      if (er && er.code === 'ELIFECYCLE') {
        /* eslint-disable standard/no-callback-literal */
        cb('Test failed.  See above for more details.')
      } else
        cb(er)
    })
  }
}
module.exports = Test
