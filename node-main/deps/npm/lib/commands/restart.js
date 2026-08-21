const LifecycleCmd = require('../lifecycle-cmd.js')

// This ends up calling run(['restart', ...args])
class Restart extends LifecycleCmd {
  static description = 'Restart a package'
  static name = 'restart'
  static params = [
    'ignore-scripts',
    'script-shell',
  ]
}

module.exports = Restart
