const t = require('tap')
const LifecycleCmd = require('../../lib/lifecycle-cmd.js')
let runArgs = null
const npm = {
  exec: async (cmd, args) => {
    if (cmd === 'run-script') {
      runArgs = args
      return 'called the right thing'
    }
  },
}
t.test('create a lifecycle command', async t => {
  t.plan(5)
  class TestStage extends LifecycleCmd {
    static get name () {
      return 'test-stage'
    }
  }
  const cmd = new TestStage(npm)
  t.match(cmd.usage, /test-stage/)
  let result
  result = await cmd.exec(['some', 'args'])
  t.same(runArgs, ['test-stage', 'some', 'args'])
  t.strictSame(result, 'called the right thing')
  result = await cmd.execWorkspaces(['some', 'args'], [])
  t.same(runArgs, ['test-stage', 'some', 'args'])
  t.strictSame(result, 'called the right thing')
})
