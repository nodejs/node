const LifecycleCmd = require('../lifecycle-cmd.js')

// This ends up calling run-script(['start', ...args])
class Start extends LifecycleCmd {
  static description = 'Start a package'
  static name = 'start'
  static params = [
    'ignore-scripts',
    'script-shell',
  ]
}
module.exports = Start
