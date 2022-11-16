const t = require('tap')

const InstallCITest = require('../../../lib/commands/install-ci-test.js')

let ciArgs = null
let ciCalled = false
let testArgs = null
let testCalled = false
let ciError = null

const installCITest = new InstallCITest({
  exec: (cmd, args) => {
    if (cmd === 'ci') {
      ciArgs = args
      ciCalled = true
    }
    if (ciError) {
      throw ciError
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

t.test('the install-ci-test command', t => {
  t.afterEach(() => {
    ciArgs = null
    ciCalled = false
    testArgs = null
    testCalled = false
    ciError = null
  })

  t.test('ci and test', async t => {
    await installCITest.exec(['extra'])
    t.equal(ciCalled, true)
    t.equal(testCalled, true)
    t.match(ciArgs, ['extra'])
    t.match(testArgs, [])
  })

  t.test('ci fails', async t => {
    ciError = new Error('test fail')
    await t.rejects(
      installCITest.exec(['extra']),
      'test fail'
    )
    t.equal(ciCalled, true)
    t.equal(testCalled, false)
    t.match(ciArgs, ['extra'])
  })
  t.end()
})
