var path = require("path")
var assert = require("assert")

process.env.npm_config_prefix = process.cwd()
delete process.env.npm_config_global
delete process.env.npm_config_depth

var npm = process.env.npm_execpath

require("child_process").execFile(process.execPath, [npm, "ls", "--json"], {
    stdio: "pipe", env: process.env, cwd: process.cwd() },
    function (err, stdout, stderr) {
  if (err) throw err

  var actual = JSON.parse(stdout)
  var expected = require("./npm-shrinkwrap.json")
  rmFrom(actual)
  actual = actual.dependencies
  rmFrom(expected)
  expected = expected.dependencies
  console.error(JSON.stringify(actual, null, 2))
  console.error(JSON.stringify(expected, null, 2))

  assert.deepEqual(actual, expected)
})

function rmFrom (obj) {
  for (var i in obj) {
    if (i === "from")
      delete obj[i]
    else if (i === "dependencies")
      for (var j in obj[i])
        rmFrom(obj[i][j])
  }
}
