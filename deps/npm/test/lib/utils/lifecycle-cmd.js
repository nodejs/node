const t = require('tap')
const LifecycleCmd = require('../../../lib/utils/lifecycle-cmd.js')
let runArgs = null
const npm = {
  commands: {
    'run-script': (args, cb) => {
      runArgs = args
      cb(null, 'called npm.commands.run')
    },
  },
}
t.test('create a lifecycle command', t => {
  t.plan(5)
  class TestStage extends LifecycleCmd {
    static get name () {
      return 'test-stage'
    }
  }
  const cmd = new TestStage(npm)
  t.match(cmd.usage, /test-stage/)
  cmd.exec(['some', 'args'], (er, result) => {
    t.same(runArgs, ['test-stage', 'some', 'args'])
    t.strictSame(result, 'called npm.commands.run')
  })
  cmd.execWorkspaces(['some', 'args'], [], (er, result) => {
    t.same(runArgs, ['test-stage', 'some', 'args'])
    t.strictSame(result, 'called npm.commands.run')
  })
})
