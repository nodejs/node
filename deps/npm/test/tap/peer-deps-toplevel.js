var npm = npm = require("../../")
var test = require("tap").test
var path = require("path")
var fs = require("fs")
var osenv = require("osenv")
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")
var common = require("../common-tap.js")

var pkg = path.resolve(__dirname, "peer-deps-toplevel")
var desiredResultsPath = path.resolve(pkg, "desired-ls-results.json")

test("installs the peer dependency directory structure", function (t) {
  mr(common.port, function (s) {
    setup(function (err) {
      t.ifError(err, "setup ran successfully")

      npm.install(".", function (err) {
        t.ifError(err, "packages were installed")

        npm.commands.ls([], true, function (err, _, results) {
          t.ifError(err, "listed tree without problems")

          fs.readFile(desiredResultsPath, function (err, desired) {
            t.ifError(err, "read desired results")

            t.deepEqual(results, JSON.parse(desired), "got expected output from ls")
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
