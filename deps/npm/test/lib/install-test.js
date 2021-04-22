const t = require('tap')

const InstallTest = require('../../lib/install-test.js')

let installArgs = null
let installCalled = false
let testArgs = null
let testCalled = false
let installError = null

const installTest = new InstallTest({
  commands: {
    install: (args, cb) => {
      installArgs = args
      installCalled = true
      cb(installError)
    },
    test: (args, cb) => {
      testArgs = args
      testCalled = true
      cb()
    },
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

  t.test('install and test', t => {
    installTest.exec(['extra'], () => {
      t.equal(installCalled, true)
      t.equal(testCalled, true)
      t.match(installArgs, ['extra'])
      t.match(testArgs, [])
      t.end()
    })
  })

  t.test('install fails', t => {
    installError = new Error('test fail')
    installTest.exec(['extra'], (err) => {
      t.equal(installCalled, true)
      t.equal(testCalled, false)
      t.match(installArgs, ['extra'])
      t.match(err, { message: 'test fail' })
      t.end()
    })
  })
  t.end()
})
