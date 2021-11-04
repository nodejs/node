const LifecycleCmd = require('../lifecycle-cmd.js')

// This ends up calling run-script(['restart', ...args])
class Restart extends LifecycleCmd {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Restart a package'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'restart'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'ignore-scripts',
      'script-shell',
    ]
  }
}
module.exports = Restart
