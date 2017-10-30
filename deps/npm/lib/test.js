module.exports = test

const testCmd = require('./utils/lifecycle-cmd.js')('test')

test.usage = testCmd.usage

function test (args, cb) {
  testCmd(args, function (er) {
    if (!er) return cb()
    if (er.code === 'ELIFECYCLE') {
      return cb('Test failed.  See above for more details.')
    }
    return cb(er)
  })
}
