const t = require('tap')
const lifecycleCmd = require('../../../lib/utils/lifecycle-cmd.js')
const npm = {
  commands: {
    'run-script': (args, cb) => cb(null, 'called npm.commands.run'),
  },
}
t.test('create a lifecycle command', t => {
  const cmd = lifecycleCmd(npm, 'asdf')
  cmd(['some', 'args'], (er, result) => {
    t.strictSame(result, 'called npm.commands.run')
    t.end()
  })
})
