module.exports = test

var testCmd = require("./utils/lifecycle.js").cmd("test")
  , log = require("npmlog")

function test (args, cb) {
  testCmd(args, function (er) {
    if (!er) return cb()
    return cb("Test failed.  See above for more details.")
  })
}
