const t = require('tap')

const InstallCITest = require('../../lib/install-ci-test.js')

let ciArgs = null
let ciCalled = false
let testArgs = null
let testCalled = false
let ciError = null

const installCITest = new InstallCITest({
  commands: {
    ci: (args, cb) => {
      ciArgs = args
      ciCalled = true
      cb(ciError)
    },
    test: (args, cb) => {
      testArgs = args
      testCalled = true
      cb()
    },
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

  t.test('ci and test', t => {
    installCITest.exec(['extra'], () => {
      t.equal(ciCalled, true)
      t.equal(testCalled, true)
      t.match(ciArgs, ['extra'])
      t.match(testArgs, [])
      t.end()
    })
  })

  t.test('ci fails', t => {
    ciError = new Error('test fail')
    installCITest.exec(['extra'], (err) => {
      t.equal(ciCalled, true)
      t.equal(testCalled, false)
      t.match(ciArgs, ['extra'])
      t.match(err, { message: 'test fail' })
      t.end()
    })
  })
  t.end()
})
