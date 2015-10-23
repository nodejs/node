// Run all the tests in the `npm-registry-couchapp` suite
// This verifies that the server-side stuff still works.

var common = require("../common-tap")
var test = require("tap").test

var npmExec = require.resolve("../../bin/npm-cli.js")
var path = require("path")
var ca = path.resolve(__dirname, "../../node_modules/npm-registry-couchapp")

var which = require("which")

var v = process.versions.node.split(".").map(function (n) { return parseInt(n, 10) })
if (v[0] === 0 && v[1] < 10) {
  console.error(
    "WARNING: need a recent Node for npm-registry-couchapp tests to run, have",
    process.versions.node
  )
}
else {
  which("couchdb", function (er) {
    if (er) {
      console.error("WARNING: need couch to run test: " + er.message)
    }
    else {
      runTests()
    }
  })
}


function runTests () {
  var env = { TAP: 1 }
  for (var i in process.env) env[i] = process.env[i]
  env.npm = npmExec

  var opts = {
    cwd: ca,
    stdio: "inherit"
  }
  common.npm(["install"], opts, function (err, code) {
    if (err) { throw err }
    if (code) {
      return test("need install to work", function (t) {
        t.fail("install failed with: " + code)
        t.end()
      })

    } else {
      opts = {
        cwd: ca,
        env: env,
        stdio: "inherit"
      }
      common.npm(["test", "--", "-Rtap"], opts, function (err, code) {
        if (err) { throw err }
        if (code) {
          return test("need test to work", function (t) {
            t.fail("test failed with: " + code)
            t.end()
          })
        }
        opts = {
          cwd: ca,
          env: env,
          stdio: "inherit"
        }
        common.npm(["prune", "--production"], opts, function (err, code) {
          if (err) { throw err }
          process.exit(code || 0)
        })
      })
    }
  })
}
