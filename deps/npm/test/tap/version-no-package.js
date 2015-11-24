var common = require("../common-tap.js")
var test = require("tap").test
var osenv = require("osenv")
var path = require("path")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")

var pkg = path.resolve(__dirname, "version-no-package")

test("setup", function (t) {
  setup()
  t.end()
})

test("npm version in a prefix with no package.json", function(t) {
  setup()
  common.npm(
    ["version", "--json", "--prefix", pkg],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, "npm version doesn't care that there's no package.json")
      t.notOk(code, "npm version ran without barfing")
      t.ok(stdout, "got version output")
      t.notOk(stderr, "no error output")
      t.doesNotThrow(function () {
        var metadata = JSON.parse(stdout)
        t.equal(metadata.node, process.versions.node, "node versions match")

      }, "able to reconstitute version object from stdout")
      t.end()
    }
  )
})

test("cleanup", function(t) {
  process.chdir(osenv.tmpdir())

  rimraf.sync(pkg)
  t.end()
})

function setup() {
  mkdirp.sync(pkg)
  process.chdir(pkg)
}
