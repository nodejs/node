var common = require("../common-tap.js")
var test = require("tap").test
var npm = require.resolve("../../bin/npm-cli.js")
var path = require("path")
var fs = require("fs")
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")

var mr = require("npm-registry-mock")

var spawn = require("child_process").spawn
var node = process.execPath

var pkg = process.env.npm_config_tmp || "/tmp"
pkg += path.sep + "noargs-install-config-save"

function writePackageJson() {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  mkdirp.sync(pkg + "/cache")

  fs.writeFileSync(pkg + "/package.json", JSON.stringify({
    "author": "Rocko Artischocko",
    "name": "noargs",
    "version": "0.0.0",
    "devDependencies": {
      "underscore": "1.3.1"
    }
  }), 'utf8')
}

function createChild (args) {
  var env = {
    npm_config_save: true,
    npm_config_registry: common.registry,
    npm_config_cache: pkg + "/cache",
    HOME: process.env.HOME,
    Path: process.env.PATH,
    PATH: process.env.PATH
  }

  if (process.platform === "win32")
    env.npm_config_cache = "%APPDATA%\\npm-cache"

  return spawn(node, args, {
    cwd: pkg,
    env: env
  })
}

test("does not update the package.json with empty arguments", function (t) {
  writePackageJson()
  t.plan(1)

  mr(common.port, function (s) {
    var child = createChild([npm, "install"])
    child.on("close", function () {
      var text = JSON.stringify(fs.readFileSync(pkg + "/package.json", "utf8"))
      t.ok(text.indexOf('"dependencies') === -1)
      s.close()
      t.end()
    })
  })
})

test("updates the package.json (adds dependencies) with an argument", function (t) {
  writePackageJson()
  t.plan(1)

  mr(common.port, function (s) {
    var child = createChild([npm, "install", "underscore"])
    child.on("close", function () {
      var text = JSON.stringify(fs.readFileSync(pkg + "/package.json", "utf8"))
      t.ok(text.indexOf('"dependencies') !== -1)
      s.close()
      t.end()
    })
  })
})

test("cleanup", function (t) {
  rimraf.sync(pkg + "/cache")
  t.end()
})
