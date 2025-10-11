const LifecycleCmd = require('../lifecycle-cmd.js')

// This ends up calling run(['stop', ...args])
class Stop extends LifecycleCmd {
  static description = 'Stop a package'
  static name = 'stop'
  static params = [
    'ignore-scripts',
    'script-shell',
  ]
}

module.exports = Stop
