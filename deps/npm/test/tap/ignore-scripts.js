var test = require("tap").test
var npm = require.resolve("../../bin/npm-cli.js")

var spawn = require("child_process").spawn
var node = process.execPath

// ignore-scripts/package.json has scripts that always exit with non-zero error
// codes. The "install" script is omitted so that npm tries to run node-gyp,
// which should also fail.
var pkg = __dirname + "/ignore-scripts"

test("ignore-scripts: install using the option", function(t) {
  createChild([npm, "install", "--ignore-scripts"]).on("close", function(code) {
    t.equal(code, 0)
    t.end()
  })
})

test("ignore-scripts: install NOT using the option", function(t) {
  createChild([npm, "install"]).on("close", function(code) {
    t.notEqual(code, 0)
    t.end()
  })
})

var scripts = [
  "prepublish", "publish", "postpublish",
  "preinstall", "install", "postinstall",
  "preuninstall", "uninstall", "postuninstall",
  "preupdate", "update", "postupdate",
  "pretest", "test", "posttest",
  "prestop", "stop", "poststop",
  "prestart", "start", "poststart",
  "prerestart", "restart", "postrestart"
]

scripts.forEach(function(script) {
  test("ignore-scripts: run-script "+script+" using the option", function(t) {
    createChild([npm, "--ignore-scripts", "run-script", script])
      .on("close", function(code) {
        t.equal(code, 0)
        t.end()
      })
  })
})

scripts.forEach(function(script) {
  test("ignore-scripts: run-script "+script+" NOT using the option", function(t) {
    createChild([npm, "run-script", script]).on("close", function(code) {
      t.notEqual(code, 0)
      t.end()
    })
  })
})

function createChild (args) {
  var env = {
    HOME: process.env.HOME,
    Path: process.env.PATH,
    PATH: process.env.PATH,
    npm_config_loglevel: "silent"
  }

  if (process.platform === "win32")
    env.npm_config_cache = "%APPDATA%\\npm-cache"

  return spawn(node, args, {
    cwd: pkg,
    stdio: "inherit",
    env: env
  })
}
