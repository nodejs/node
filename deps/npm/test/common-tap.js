var spawn = require("child_process").spawn

var port = exports.port = 1337
exports.registry = "http://localhost:" + port
process.env.npm_config_loglevel = "error"

var bin = exports.bin = require.resolve("../bin/npm-cli.js")
var once = require("once")
exports.npm = function (cmd, opts, cb) {
  cb = once(cb)
  cmd = [bin].concat(cmd)
  opts = opts || {}

  var stdout = ""
    , stderr = ""
    , node = process.execPath
    , child = spawn(node, cmd, opts)

  if (child.stderr) child.stderr.on("data", function (chunk) {
    stderr += chunk
  })

  if (child.stdout) child.stdout.on("data", function (chunk) {
    stdout += chunk
  })

  child.on("error", cb)

  child.on("close", function (code) {
    cb(null, code, stdout, stderr)
  })
}
