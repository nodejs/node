const LifecycleCmd = require('./utils/lifecycle-cmd.js')

// This ends up calling run-script(['restart', ...args])
class Restart extends LifecycleCmd {
  constructor (npm) {
    super(npm, 'restart')
  }
}
module.exports = Restart
