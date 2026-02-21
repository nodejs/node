const t = require('tap')
const mockNpm = require('../fixtures/mock-npm')
const LifecycleCmd = require('../../lib/lifecycle-cmd.js')

t.test('create a lifecycle command', async t => {
  let runArgs = null
  const { npm } = await mockNpm(t)
  npm.exec = async (cmd, args) => {
    if (cmd === 'run') {
      runArgs = args
      return 'called the right thing'
    }
  }

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

  result = await cmd.execWorkspaces(['some', 'args'])
  t.same(runArgs, ['test-stage', 'some', 'args'])
  t.strictSame(result, 'called the right thing')
})
