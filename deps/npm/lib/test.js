module.exports = test

const testCmd = require('./utils/lifecycle.js').cmd('test')
const usage = require('./utils/usage')

test.usage = usage(
  'test',
  'npm test [-- <args>]'
)

function test (args, cb) {
  testCmd(args, function (er) {
    if (!er) return cb()
    if (er.code === 'ELIFECYCLE') {
      return cb('Test failed.  See above for more details.')
    }
    return cb(er)
  })
}
