var test = require("tap").test
var npm = require.resolve("../../bin/npm-cli.js")
var node = process.execPath
var spawn = require("child_process").spawn
var path = require("path")
var pkg = path.resolve(__dirname, "lifecycle-signal")

test("lifecycle signal abort", function (t) {
  var child = spawn(node, [npm, "install"], {
    cwd: pkg
  })
  child.on("close", function (code, signal) {
    t.equal(code, null)
    t.equal(signal, "SIGSEGV")
    t.end()
  })
})
