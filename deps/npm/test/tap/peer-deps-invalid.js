var fs = require("fs")
var test = require("tap").test
var rimraf = require("rimraf")
var npm = require("../../")

test("installing dependencies that having conflicting peerDependencies", function (t) {
  t.plan(1)

  rimraf.sync(__dirname + "/peer-deps-invalid/node_modules")
  process.chdir(__dirname + "/peer-deps-invalid")

  npm.load(function () {
    npm.commands.install([], function (err) {
      if (!err) {
        t.fail("No error!")
        process.exit(1)
        return
      }

      t.equal(err.code, "EPEERINVALID")
      process.exit(0)
    })
  })
})
