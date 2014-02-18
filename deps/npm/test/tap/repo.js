if (process.platform === "win32") {
  console.error("skipping test, because windows and shebangs")
  return
}

var common = require("../common-tap.js")
var mr = require("npm-registry-mock")

var test = require("tap").test
var npm = require.resolve("../../bin/npm-cli.js")
var node = process.execPath
var rimraf = require("rimraf")
var spawn = require("child_process").spawn
var fs = require("fs")

test("setup", function (t) {
  var s = "#!/usr/bin/env bash\n" +
          "echo \"$@\" > " + JSON.stringify(__dirname) + "/_output\n"
  fs.writeFileSync(__dirname + "/_script.sh", s, "ascii")
  fs.chmodSync(__dirname + "/_script.sh", "0755")
  t.pass("made script")
  t.end()
})

test("npm repo underscore", function (t) {
  mr(common.port, function (s) {
    var c = spawn(node, [
      npm, "repo", "underscore",
      "--registry=" + common.registry,
      "--loglevel=silent",
      "--browser=" + __dirname + "/_script.sh",
    ])
    c.stdout.on("data", function(d) {
      t.fail("Should not get data on stdout: " + d)
    })
    c.stderr.pipe(process.stderr)
    c.on("close", function(code) {
      t.equal(code, 0, "exit ok")
      var res = fs.readFileSync(__dirname + "/_output", "ascii")
      s.close()
      t.equal(res, "https://github.com/jashkenas/underscore\n")
      t.end()
    })
  })
})

test("cleanup", function (t) {
  fs.unlinkSync(__dirname + "/_script.sh")
  fs.unlinkSync(__dirname + "/_output")
  t.pass("cleaned up")
  t.end()
})
