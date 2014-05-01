var test = require("tap").test
  , npm = require("../../")
  , mr = require("npm-registry-mock")
  , common = require("../common-tap.js")
  , path = require("path")
  , fs = require("fs")
  , osenv = require("osenv")
  , rimraf = require("rimraf")
  , pkg = __dirname + "/shrinkwrap-empty-deps"

test("returns a list of removed items", function (t) {
  var desiredResultsPath = path.resolve(pkg, "npm-shrinkwrap.json")

  cleanup()

  mr(common.port, function (s) {
    setup(function () {
      npm.shrinkwrap([], function (err) {
        if (err) return t.fail(err)
        fs.readFile(desiredResultsPath, function (err, desired) {
          if (err) return t.fail(err)
          t.deepEqual({
            "name": "npm-test-shrinkwrap-empty-deps",
            "version": "0.0.0",
            "dependencies": {}
          }, JSON.parse(desired))
          cleanup()
          s.close()
          t.end()
        })
      })
    })
  })
})

function setup (cb) {
  cleanup()
  process.chdir(pkg)
  npm.load({cache: pkg + "/cache", registry: common.registry}, function () {
    cb()
  })
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(path.resolve(pkg, "npm-shrinkwrap.json"))
}
