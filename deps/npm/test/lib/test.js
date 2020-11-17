const t = require('tap')
const requireInject = require('require-inject')
let RUN_ARGS = null
const npmock = {
  commands: {
    run: (args, cb) => {
      RUN_ARGS = args
      cb()
    },
  },
}
const test = requireInject('../../lib/test.js', {
  '../../lib/npm.js': npmock,
})

t.test('run a test', t => {
  test([], (er) => {
    t.strictSame(RUN_ARGS, ['test'], 'added "test" to the args')
  })
  test(['hello', 'world'], (er) => {
    t.strictSame(RUN_ARGS, ['test', 'hello', 'world'], 'added positional args')
  })

  const lcErr = Object.assign(new Error('should not see this'), {
    code: 'ELIFECYCLE',
  })
  const otherErr = new Error('should see this')

  npmock.commands.run = (args, cb) => cb(lcErr)
  test([], (er) => {
    t.equal(er, 'Test failed.  See above for more details.')
  })

  npmock.commands.run = (args, cb) => cb(otherErr)
  test([], (er) => {
    t.match(er, { message: 'should see this' })
  })

  t.end()
})
