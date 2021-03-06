const LifecycleCmd = require('./utils/lifecycle-cmd.js')

// This ends up calling run-script(['stop', ...args])
class Stop extends LifecycleCmd {
  constructor (npm) {
    super(npm, 'stop')
  }
}
module.exports = Stop
