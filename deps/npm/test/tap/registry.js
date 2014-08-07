// Run all the tests in the `npm-registry-couchapp` suite
// This verifies that the server-side stuff still works.

var test = require("tap").test

var spawn = require("child_process").spawn
var npmExec = require.resolve("../../bin/npm-cli.js")
var path = require("path")
var ca = path.resolve(__dirname, "../../node_modules/npm-registry-couchapp")

var which = require("which")
var hasCouch = false

which("couchdb", function(er, couch) {
  if (er) {
    return test("need couchdb", function (t) {
      t.fail("need couch to run test: " + er.message)
      t.end()
    })
  } else {
    runTests()
  }
})

function runTests () {
  spawn(process.execPath, [
    npmExec, "install"
  ], {
    cwd: ca,
    stdio: "inherit"
  }).on("close", function (code, sig) {
    if (code || sig) {
      return test("need install to work", function (t) {
        t.fail("install failed with: " (code || sig))
        t.end()
      })

    } else {
      var env = {}
      for (var i in process.env) env[i] = process.env[i]
      env.npm = npmExec

      spawn(process.execPath, [
        npmExec, "test"
      ], {
        cwd: ca,
        env: env,
        stdio: "inherit"
      }).on("close", function (code, sig) {
        process.exit(code || sig)
      })
    }

  })
}
