'use strict'

// npm install-test
// Runs `npm install` and then runs `npm test`

module.exports = installTest
var install = require('./install.js')
var test = require('./test.js')
var usage = require('./utils/usage')

installTest.usage = usage(
  'install-test',
  '\nnpm install-test [args]' +
  '\nSame args as `npm install`'
)

installTest.completion = install.completion

function installTest (args, cb) {
  install(args, function (er) {
    if (er) {
      return cb(er)
    }
    test([], cb)
  })
}
