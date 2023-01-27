const t = require('tap')

const InstallTest = require('../../../lib/commands/install-test.js')

let installArgs = null
let installCalled = false
let testArgs = null
let testCalled = false
let installError = null

const installTest = new InstallTest({
  exec: (cmd, args) => {
    if (cmd === 'install') {
      installArgs = args
      installCalled = true
    }
    if (installError) {
      throw installError
    }

    if (cmd === 'test') {
      testArgs = args
      testCalled = true
    }
  },
  config: {
    validate: () => {},
    get: (key) => {
      if (key === 'location') {
        return 'project'
      }
    },
    isDefault: () => {},
  },
})

t.test('the install-test command', t => {
  t.afterEach(() => {
    installArgs = null
    installCalled = false
    testArgs = null
    testCalled = false
    installError = null
  })

  t.test('install and test', async t => {
    await installTest.exec(['extra'])
    t.equal(installCalled, true)
    t.equal(testCalled, true)
    t.match(installArgs, ['extra'])
    t.match(testArgs, [])
  })

  t.test('install fails', async t => {
    installError = new Error('test fail')
    await t.rejects(
      installTest.exec(['extra']),
      'test fail'
    )
    t.equal(installCalled, true)
    t.equal(testCalled, false)
    t.match(installArgs, ['extra'])
  })
  t.end()
})
