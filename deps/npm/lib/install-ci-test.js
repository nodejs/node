'use strict'

// npm install-ci-test
// Runs `npm ci` and then runs `npm test`

module.exports = installTest
var ci = require('./ci.js')
var test = require('./test.js')
var usage = require('./utils/usage')

installTest.usage = usage(
  'install-ci-test',
  '\nnpm install-ci-test [args]' +
  '\nSame args as `npm ci`'
)

installTest.completion = ci.completion

function installTest (args, cb) {
  ci(args, function (er) {
    if (er) {
      return cb(er)
    }
    test([], cb)
  })
}
