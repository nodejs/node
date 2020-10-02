// npm install-ci-test
// Runs `npm ci` and then runs `npm test`

const ci = require('./ci.js')
const test = require('./test.js')
const usageUtil = require('./utils/usage.js')

const usage = usageUtil(
  'install-ci-test',
  'npm install-ci-test [args]' +
  '\nSame args as `npm ci`'
)

const completion = ci.completion

const ciTest = (args, cb) =>
  ci(args, er => er ? cb(er) : test([], cb))

module.exports = Object.assign(ciTest, { usage, completion })
