var test = require("tap").test
  , fs = require("fs")
  , path = require("path")
  , existsSync = fs.existsSync || path.existsSync
  , npm = require("../../")
  , rimraf = require("rimraf")
  , mr = require("npm-registry-mock")
  , common = require('../common-tap.js')

test("dedupe finds the common module and moves it up one level", function (t) {
  setup(function (s) {
    npm.install(".", function (err) {
      if (err) return t.fail(err)
      npm.dedupe(function(err) {
        if (err) return t.fail(err)
        t.ok(existsSync(path.join(__dirname, "dedupe", "node_modules", "minimist")))
        t.ok(!existsSync(path.join(__dirname, "dedupe", "node_modules", "checker")))
        s.close() // shutdown mock registry.
        t.end()
      })
    })
  })
})

function setup (cb) {
  process.chdir(path.join(__dirname, "dedupe"))
  mr(common.port, function (s) { // create mock registry.
    npm.load({registry: common.registry}, function() {
      rimraf.sync(path.join(__dirname, "dedupe", "node_modules"))
      fs.mkdirSync(path.join(__dirname, "dedupe", "node_modules"))
      cb(s)
    })
  })
}
