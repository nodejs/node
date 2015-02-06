var test = require("tap").test
  , fs = require("fs")
  , path = require("path")
  , existsSync = fs.existsSync || path.existsSync
  , npm = require("../../")
  , rimraf = require("rimraf")
  , osenv = require("osenv")
  , mr = require("npm-registry-mock")
  , common = require("../common-tap.js")
  , server

var pkg = path.resolve(__dirname, "circular-dep")

test("installing a package that depends on the current package", function (t) {
  t.plan(1)

  setup(function () {
    npm.install("optimist", function (err) {
      if (err) return t.fail(err)
      npm.dedupe(function (err) {
        if (err) return t.fail(err)
        t.ok(existsSync(path.resolve(pkg,
          "minimist", "node_modules", "optimist",
          "node_modules", "minimist"
        )), "circular dependency uncircled")
        cleanup()
        server.close()
      })
    })
  })
})

function setup (cb) {
  cleanup()
  process.chdir(path.resolve(pkg, "minimist"))

  fs.mkdirSync(path.resolve(pkg, "minimist/node_modules"))
  mr({port : common.port}, function (er, s) {
    server = s
    npm.load({
      loglevel: "silent",
      registry: common.registry,
      cache: path.resolve(pkg, "cache")
    }, cb)
  })
}

function cleanup() {
  process.chdir(osenv.tmpdir())
  rimraf.sync(path.resolve(pkg, "minimist/node_modules"))
  rimraf.sync(path.resolve(pkg, "cache"))
}
