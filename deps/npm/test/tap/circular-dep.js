var test = require("tap").test
  , fs = require("fs")
  , path = require("path")
  , existsSync = fs.existsSync || path.existsSync
  , npm = require("../../")
  , rimraf = require("rimraf")
  , mr = require("npm-registry-mock")
  , common = require("../common-tap.js")
  , server

test("installing a package that depends on the current package", function (t) {
  t.plan(1)

  setup(function () {
    npm.install("optimist", function (err) {
      if (err) return t.fail(err)
      npm.dedupe(function(err) {
        if (err) return t.fail(err)
        t.ok(existsSync(path.join(__dirname,
          "circular-dep", "minimist", "node_modules", "optimist",
          "node_modules", "minimist"
        )))
        cleanup()
        server.close()
      })
    })
  })
})

function setup (cb) {
  process.chdir(path.join(__dirname, "circular-dep", "minimist"))
  cleanup()
  fs.mkdirSync(path.join(__dirname,
    "circular-dep", "minimist", "node_modules"))
  mr(common.port, function (s) {
    server = s
    npm.load({
      registry: common.registry,
      cache: path.resolve(__dirname, "circular-dep", "cache")
    }, cb)
  })
}

function cleanup() {
  rimraf.sync(path.join(__dirname,
    "circular-dep", "minimist", "node_modules"))
  rimraf.sync(path.join(__dirname,
    "circular-dep", "cache"))
}
