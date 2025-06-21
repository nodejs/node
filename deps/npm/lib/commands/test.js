const LifecycleCmd = require('../lifecycle-cmd.js')

// This ends up calling run(['test', ...args])
class Test extends LifecycleCmd {
  static description = 'Test a package'
  static name = 'test'
  static params = [
    'ignore-scripts',
    'script-shell',
  ]
}

module.exports = Test
