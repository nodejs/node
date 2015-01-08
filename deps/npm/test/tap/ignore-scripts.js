var common = require("../common-tap")
var test = require("tap").test
var path = require("path")

// ignore-scripts/package.json has scripts that always exit with non-zero error
// codes. The "install" script is omitted so that npm tries to run node-gyp,
// which should also fail.
var pkg = path.resolve(__dirname, "ignore-scripts")

test("ignore-scripts: install using the option", function (t) {
  createChild(["install", "--ignore-scripts"], function (err, code) {
    t.ifError(err, "install with scripts ignored finished successfully")
    t.equal(code, 0, "npm install exited with code")
    t.end()
  })
})

test("ignore-scripts: install NOT using the option", function (t) {
  createChild(["install"], function (err, code) {
    t.ifError(err, "install with scripts successful")
    t.notEqual(code, 0, "npm install exited with code")
    t.end()
  })
})

var scripts = [
  "prepublish", "publish", "postpublish",
  "preinstall", "install", "postinstall",
  "preuninstall", "uninstall", "postuninstall",
  "pretest", "test", "posttest",
  "prestop", "stop", "poststop",
  "prestart", "start", "poststart",
  "prerestart", "restart", "postrestart"
]

scripts.forEach(function (script) {
  test("ignore-scripts: run-script "+script+" using the option", function (t) {
    createChild(["--ignore-scripts", "run-script", script], function (err, code) {
      t.ifError(err, "run-script " + script + " with ignore-scripts successful")
      t.equal(code, 0, "npm run-script exited with code")
      t.end()
    })
  })
})

scripts.forEach(function (script) {
  test("ignore-scripts: run-script "+script+" NOT using the option", function (t) {
    createChild(["run-script", script], function (err, code) {
      t.ifError(err, "run-script " + script + " finished successfully")
      t.notEqual(code, 0, "npm run-script exited with code")
      t.end()
    })
  })
})

function createChild (args, cb) {
  var env = {
    HOME: process.env.HOME,
    Path: process.env.PATH,
    PATH: process.env.PATH,
    "npm_config_loglevel": "silent"
  }

  if (process.platform === "win32")
    env.npm_config_cache = "%APPDATA%\\npm-cache"

  return common.npm(args, {
    cwd: pkg,
    stdio: "inherit",
    env: env
  }, cb)
}
