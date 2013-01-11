var path = require("path")
var assert = require("assert")

process.env.npm_config_prefix = process.cwd()
delete process.env.npm_config_global
delete process.env.npm_config_depth

var npm = process.env.npm_execpath

require("child_process").exec(npm + " ls --json", {
    env: process.env, cwd: process.cwd() },
    function (err, stdout, stderr) {

  var actual = JSON.parse(stdout).dependencies
  var expected = require("./npm-ls.json").dependencies

  // Delete the "problems" entry because it contains system-specific path info,
  // so we can't compare it accurately and thus have deleted it from
  // ./npm-ls.json.
  delete actual.dict.problems

  // It's undefined which peerDependency will get installed first, so
  // this will be either version 1.1.0 or version 1.0.0
  var dictVer = actual.dict.version
  delete actual.dict.version
  assert(dictVer === "1.1.0" || dictVer === "1.0.0")
  assert.deepEqual(actual, expected)

  assert.ok(err)
  assert(/peer invalid/.test(err.message))
})
