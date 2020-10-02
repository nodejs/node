// npm install-test
// Runs `npm install` and then runs `npm test`

const install = require('./install.js')
const test = require('./test.js')
const usageUtil = require('./utils/usage.js')

const usage = usageUtil(
  'install-test',
  'npm install-test [args]' +
  '\nSame args as `npm install`'
)

const completion = install.completion

const installTest = (args, cb) =>
  install(args, er => er ? cb(er) : test([], cb))

module.exports = Object.assign(installTest, { usage, completion })
