const LifecycleCmd = require('./utils/lifecycle-cmd.js')

// This ends up calling run-script(['start', ...args])
class Start extends LifecycleCmd {
  constructor (npm) {
    super(npm, 'start')
  }
}
module.exports = Start
