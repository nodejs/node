var path = require("path")
var assert = require("assert")

process.env.npm_config_prefix = process.cwd()
delete process.env.npm_config_global
delete process.env.npm_config_depth

var npm = process.platform === "win32"
        ? path.resolve(process.env.npm_config_prefix, "../../npm")
        : path.resolve(process.env.npm_config_prefix, "../../../bin/npm")

require("child_process").exec(npm + " ls --json", {
    stdio: "pipe", env: process.env, cwd: process.cwd() },
    function (err, stdout, stderr) {
  if (err) throw err

  var actual = JSON.parse(stdout).dependencies
  var expected = require("./npm-shrinkwrap.json").dependencies

  assert.deepEqual(actual, expected)
})
