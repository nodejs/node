var fs = require("fs")
var test = require("tap").test
var rimraf = require("rimraf")
var npm = require("../../")

var peerDepsTestUrl = "https://gist.github.com/raw/3971128/3f6aa37b4fa1186c2f47da9b77dcc4ec496e3483/index.js"

test("installing a peerDependencies-using package without a package.json present (GH-3049)", function (t) {

  rimraf.sync(__dirname + "/peer-deps-without-package-json/node_modules")
  fs.mkdirSync(__dirname + "/peer-deps-without-package-json/node_modules")
  process.chdir(__dirname + "/peer-deps-without-package-json")

  npm.load(function () {
    npm.install(peerDepsTestUrl, function (err) {
      if (err) {
        t.fail(err)
        t.end()
        process.exit(1)
        return
      }

      t.ok(fs.existsSync(__dirname + "/peer-deps-without-package-json/node_modules/npm-test-peer-deps-file"))
      t.ok(fs.existsSync(__dirname + "/peer-deps-without-package-json/node_modules/dict"))
      t.end()
      process.exit(0)
    })
  })
})
