const t = require('tap')
let RUN_ARGS = null
const npm = {
  commands: {
    'run-script': (args, cb) => {
      RUN_ARGS = args
      cb()
    },
  },
}
const Test = require('../../lib/test.js')
const test = new Test(npm)

t.test('run a test', t => {
  test.exec([], (er) => {
    t.strictSame(RUN_ARGS, ['test'], 'added "test" to the args')
  })
  test.exec(['hello', 'world'], (er) => {
    t.strictSame(RUN_ARGS, ['test', 'hello', 'world'], 'added positional args')
  })

  const lcErr = Object.assign(new Error('should not see this'), {
    code: 'ELIFECYCLE',
  })
  const otherErr = new Error('should see this')

  npm.commands['run-script'] = (args, cb) => cb(lcErr)
  test.exec([], (er) => {
    t.equal(er, 'Test failed.  See above for more details.')
  })

  npm.commands['run-script'] = (args, cb) => cb(otherErr)
  test.exec([], (er) => {
    t.match(er, { message: 'should see this' })
  })

  t.end()
})
