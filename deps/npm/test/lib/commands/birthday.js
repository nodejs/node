const t = require('tap')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

t.test('birthday', async t => {
  t.plan(4)
  const config = {
    yes: false,
    package: [],
  }
  const npm = mockNpm({
    config,
    cmd: async (cmd) => {
      t.ok(cmd, 'exec', 'calls out to exec command')
      return {
        exec: async (args) => {
          t.equal(npm.config.get('yes'), true, 'should say yes')
          t.strictSame(npm.config.get('package'), ['@npmcli/npm-birthday'],
            'uses correct package')
          t.strictSame(args, ['npm-birthday'], 'called with correct args')
        },
      }
    },
  })
  const Birthday = require('../../../lib/commands/birthday.js')
  const birthday = new Birthday(npm)

  await birthday.exec([])
})
