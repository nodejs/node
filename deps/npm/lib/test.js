module.exports = test

var testCmd = require("./utils/lifecycle.js").cmd("test")

function test (args, cb) {
  testCmd(args, function (er) {
    if (!er) return cb()
    if (er.code === "ELIFECYCLE") {
      return cb("Test failed.  See above for more details.")
    }
    return cb(er)
  })
}
