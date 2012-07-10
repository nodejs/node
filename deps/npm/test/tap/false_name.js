var test = require("tap").test
  , fs = require("fs")
  , path = require("path")
  , existsSync = fs.existsSync || path.existsSync
  , spawn = require("child_process").spawn
  , npm = require("../../")

test("not every pkg.name can be required", function (t) {
  t.plan(1)

  setup(function () {
    npm.install(".", function (err) {
      if (err) return t.fail(err)
      t.ok(existsSync(__dirname +
        "/false_name/node_modules/tap/node_modules/buffer-equal"))
    })
  })
})

function setup (cb) {
  process.chdir(__dirname + "/false_name")
  npm.load(function () {
    spawn("rm", [ "-rf", __dirname + "/false_name/node_modules" ])
      .on("exit", function () {
        fs.mkdirSync(__dirname + "/false_name/node_modules")
        cb()
      })
  })
}
