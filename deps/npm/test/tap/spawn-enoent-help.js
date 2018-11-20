var path = require("path")
var test = require("tap").test
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")
var common = require("../common-tap.js")

var pkg = path.resolve(__dirname, "spawn-enoent-help")

test("setup", function (t) {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  t.end()
})

test("enoent help", function (t) {
  common.npm(["help", "config"], {
    cwd: pkg,
    env: {
      PATH: "",
      Path: "",
      "npm_config_loglevel": "warn",
      "npm_config_viewer": "woman"
    }
  }, function (er, code, sout, serr) {
    t.similar(serr, /Check if the file 'emacsclient' is present./)
    t.equal(global.cooked, undefined, "Don't leak into global scope")
    t.end()
  })
})

test("clean", function (t) {
  rimraf.sync(pkg)
  t.end()
})
