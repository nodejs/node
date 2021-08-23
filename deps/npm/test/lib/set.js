const t = require('tap')

// can't run this until npm set can save to npm.localPrefix
t.skip('npm set', async t => {
  const { real: mockNpm } = require('../fixtures/mock-npm')
  const { joinedOutput, command, npm } = mockNpm(t)
  await npm.load()

  t.test('no args', async t => {
    t.rejects(
      command('set'),
      /Usage:/,
      'prints usage'
    )
  })

  t.test('test-config-item', async t => {
    npm.localPrefix = t.testdir({})
    t.not(npm.config.get('test-config-item', 'project'), 'test config value', 'config is not already new value')
    // This will write to ~/.npmrc!
    // Don't unskip until we can write to project level
    await command('set', ['test-config-item=test config value'])
    t.equal(joinedOutput(), '', 'outputs nothing')
    t.equal(npm.config.get('test-config-item', 'project'), 'test config value', 'config is set to new value')
  })
})

// Everything after this can go away once the above test is unskipped

let configArgs = null
const npm = {
  commands: {
    config: (args, cb) => {
      configArgs = args
      cb()
    },
  },
}

const Set = t.mock('../../lib/set.js')
const set = new Set(npm)

t.test('npm set - no args', t => {
  set.exec([], (err) => {
    t.match(err, /npm set/, 'prints usage')
    t.end()
  })
})

t.test('npm set', t => {
  set.exec(['email', 'me@me.me'], (err) => {
    if (err)
      throw err

    t.strictSame(configArgs, ['set', 'email', 'me@me.me'], 'passed the correct arguments to config')
    t.end()
  })
})
