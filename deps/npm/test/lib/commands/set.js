const t = require('tap')

// can't run this until npm set can save to project level npmrc
t.skip('npm set', async t => {
  // XXX: convert to loadMockNpm
  const { real: mockNpm } = require('../../fixtures/mock-npm')
  const { joinedOutput, Npm } = mockNpm(t)
  const npm = new Npm()
  await npm.load()

  t.test('no args', async t => {
    t.rejects(npm.exec('set', []), /Usage:/, 'prints usage')
  })

  t.test('test-config-item', async t => {
    npm.localPrefix = t.testdir({})
    t.not(
      npm.config.get('test-config-item', 'project'),
      'test config value',
      'config is not already new value'
    )
    // This will write to ~/.npmrc!
    // Don't unskip until we can write to project level
    await npm.exec('set', ['test-config-item=test config value'])
    t.equal(joinedOutput(), '', 'outputs nothing')
    t.equal(
      npm.config.get('test-config-item', 'project'),
      'test config value',
      'config is set to new value'
    )
  })
})

// Everything after this can go away once the above test is unskipped

let configArgs = null
const npm = {
  exec: async (cmd, args) => {
    if (cmd === 'config') {
      configArgs = args
    }
  },
}

const Set = t.mock('../../../lib/commands/set.js')
const set = new Set(npm)

t.test('npm set - no args', async t => {
  await t.rejects(set.exec([]), set.usage)
})

t.test('npm set', async t => {
  await set.exec(['email', 'me@me.me'])

  t.strictSame(configArgs, ['set', 'email', 'me@me.me'], 'passed the correct arguments to config')
})
