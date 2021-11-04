const LifecycleCmd = require('../lifecycle-cmd.js')

// This ends up calling run-script(['start', ...args])
class Start extends LifecycleCmd {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Start a package'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'start'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'ignore-scripts',
      'script-shell',
    ]
  }
}
module.exports = Start
