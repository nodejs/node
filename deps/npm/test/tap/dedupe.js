var test = require("tap").test
  , fs = require("fs")
  , path = require("path")
  , existsSync = fs.existsSync || path.existsSync
  , npm = require("../../")
  , rimraf = require("rimraf")

test("dedupe finds the common module and moves it up one level", function (t) {
  t.plan(2)

  setup(function () {
    npm.install(".", function (err) {
      if (err) return t.fail(err)
      npm.dedupe(function(err) {
        if (err) return t.fail(err)
        t.ok(existsSync(path.join(__dirname, "dedupe", "node_modules", "minimist")))
        t.ok(!existsSync(path.join(__dirname, "dedupe", "node_modules", "prime")))
      })
    })
  })
})

function setup (cb) {
  process.chdir(path.join(__dirname, "dedupe"))
  npm.load(function () {
    rimraf.sync(path.join(__dirname, "dedupe", "node_modules"))
    fs.mkdirSync(path.join(__dirname, "dedupe", "node_modules"))
    cb()
  })
}
