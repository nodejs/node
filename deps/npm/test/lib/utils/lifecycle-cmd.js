const t = require('tap')
const requireInject = require('require-inject')
const lifecycleCmd = requireInject('../../../lib/utils/lifecycle-cmd.js', {
  '../../../lib/npm.js': {
    commands: {
      run: (args, cb) => cb(null, 'called npm.commands.run')
    }
  }
})

t.test('create a lifecycle command', t => {
  const cmd = lifecycleCmd('asdf')
  t.equal(cmd.completion, require('../../../lib/utils/completion/none.js'), 'empty completion')
  cmd(['some', 'args'], (er, result) => {
    t.strictSame(result, 'called npm.commands.run')
    t.end()
  })
})
