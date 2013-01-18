var path = require("path")
var assert = require("assert")

process.env.npm_config_prefix = process.cwd()
delete process.env.npm_config_global
delete process.env.npm_config_depth

var npm = process.env.npm_execpath

require("child_process").exec(npm + " ls --json", {
    env: process.env, cwd: process.cwd() },
    function (err, stdout, stderr) {

  if (err) throw err

  var actual = JSON.parse(stdout).dependencies
  var expected = require("./npm-ls.json")
  console.error(JSON.stringify(actual, null, 2))
  console.error(JSON.stringify(expected, null, 2))

  assert.deepEqual(actual, expected)
})
