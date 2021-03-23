const LifecycleCmd = require('./utils/lifecycle-cmd.js')

// This ends up calling run-script(['stop', ...args])
class Stop extends LifecycleCmd {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Stop a package'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'stop'
  }
}
module.exports = Stop
