const testCmd = require('./utils/lifecycle-cmd.js')('test')
const { completion, usage } = testCmd
const cmd = (args, cb) => testCmd(args, er => {
  if (er && er.code === 'ELIFECYCLE') {
    /* eslint-disable standard/no-callback-literal */
    cb('Test failed.  See above for more details.')
  } else
    cb(er)
})

module.exports = Object.assign(cmd, { completion, usage })
