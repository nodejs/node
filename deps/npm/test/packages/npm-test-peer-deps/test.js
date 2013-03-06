var path = require("path")
var assert = require("assert")

process.env.npm_config_prefix = process.cwd()
delete process.env.npm_config_global
delete process.env.npm_config_depth

var npm = process.env.npm_execpath

require("child_process").execFile(process.execPath, [npm, "ls", "--json"], {
    env: process.env, cwd: process.cwd() },
    function (err, stdout, stderr) {

  if (err) throw err

  var actual = JSON.parse(stdout).dependencies
  var expected = require("./npm-ls.json")

  // resolved url doesn't matter
  clean(actual)
  clean(expected)

  console.error(JSON.stringify(actual, null, 2))
  console.error(JSON.stringify(expected, null, 2))

  assert.deepEqual(actual, expected)
})

function clean (obj) {
  for (var i in obj) {
    if (i === "from" || i === "resolved")
      delete obj[i]
    else if (typeof obj[i] === "object" && obj[i])
      clean(obj[i])
  }
}
