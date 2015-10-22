var test = require("tap").test
var spawn = require("child_process").spawn
var node = process.execPath
var npm = require.resolve("../../")
var args = [ npm, "ls", "ceci n’est pas une package" ]
test("ls exits non-zero when nothing found", function (t) {
  var child = spawn(node, args)
  child.on("exit", function (code) {
    t.notEqual(code, 0)
    t.end()
  })
})
