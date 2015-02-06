var npm = npm = require("../../")
var test = require("tap").test
var path = require("path")
var fs = require("fs")
var osenv = require("osenv")
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")
var common = require("../common-tap.js")

var pkg = path.resolve(__dirname, "dev-dep-duplicate")
var desiredResultsPath = path.resolve(pkg, "desired-ls-results.json")

test("prefers version from dependencies over devDependencies", function (t) {
  t.plan(1)

  mr({port : common.port}, function (er, s) {
    setup(function (err) {
      if (err) return t.fail(err)

      npm.install(".", function (err) {
        if (err) return t.fail(err)

        npm.commands.ls([], true, function (err, _, results) {
          if (err) return t.fail(err)

          fs.readFile(desiredResultsPath, function (err, desired) {
            if (err) return t.fail(err)

            t.deepEqual(results, JSON.parse(desired))
            s.close()
            t.end()
          })
        })
      })
    })
  })
})

test("cleanup", function (t) {
  cleanup()
  t.end()
})


function setup (cb) {
  cleanup()
  process.chdir(pkg)

  var opts = { cache: path.resolve(pkg, "cache"), registry: common.registry}
  npm.load(opts, cb)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(path.resolve(pkg, "node_modules"))
  rimraf.sync(path.resolve(pkg, "cache"))
}
