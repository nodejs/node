var fs = require("fs")
var path = require("path")

var test = require("tap").test
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")

var npm = require("../../")
var common = require("../common-tap.js")

var pkg = path.join(__dirname, "publish-access-unscoped")

// TODO: nock uses setImmediate, breaks 0.8: replace with mockRegistry
if (!global.setImmediate) {
  global.setImmediate = function () {
    var args = [arguments[0], 0].concat([].slice.call(arguments, 1))
    setTimeout.apply(this, args)
  }
}

test("setup", function (t) {
  mkdirp(path.join(pkg, "cache"), function () {
    var configuration = {
      cache    : path.join(pkg, "cache"),
      loglevel : "silent",
      registry : common.registry
    }

    npm.load(configuration, next)
  })

  function next (er) {
    t.ifError(er, "npm loaded successfully")

    process.chdir(pkg)
    fs.writeFile(
      path.join(pkg, "package.json"),
      JSON.stringify({
        name: "publish-access",
        version: "1.2.5"
      }),
      "ascii",
      function (er) {
        t.ifError(er)

        t.pass("setup done")
        t.end()
      }
    )
  }
})

test("unscoped packages cannot be restricted", function (t) {
  npm.config.set("access", "restricted")
  npm.commands.publish([], false, function (er) {
    t.ok(er, "got an error back")
    t.equal(er.message, "Can't restrict access to unscoped packages.")

    t.end()
  })
})

test("cleanup", function (t) {
  process.chdir(__dirname)
  rimraf(pkg, function (er) {
    t.ifError(er)

    t.end()
  })
})
