var test = require("tap").test
var common = require("../common-tap.js")
var path = require("path")
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")
var pkg = path.resolve(__dirname, "lifecycle-path")
var fs = require("fs")
var link = path.resolve(pkg, "node-bin")

// Without the path to the shell, nothing works usually.
var PATH
if (process.platform === "win32") {
  PATH = "C:\\Windows\\system32;C:\\Windows"
} else {
  PATH = "/bin:/usr/bin"
}

test("setup", function (t) {
  rimraf.sync(link)
  fs.symlinkSync(path.dirname(process.execPath), link, "dir")
  t.end()
})

test("make sure the path is correct", function (t) {
  common.npm(["run-script", "path"], {
    cwd: pkg,
    env: {
      PATH: PATH,
      stdio: [ 0, "pipe", 2 ]
    }
  }, function (er, code, stdout, stderr) {
    if (er) throw er
    t.equal(code, 0, "exit code")
    // remove the banner, we just care about the last line
    stdout = stdout.trim().split(/\r|\n/).pop()
    var pathSplit = process.platform === "win32" ? ";" : ":"
    var root = path.resolve(__dirname, "../..")
    var actual = stdout.split(pathSplit).map(function (p) {
      if (p.indexOf(root) === 0) {
        p = "{{ROOT}}" + p.substr(root.length)
      }
      return p.replace(/\\/g, "/")
    })

    // get the ones we tacked on, then the system-specific requirements
    var expect = [
      "{{ROOT}}/bin/node-gyp-bin",
      "{{ROOT}}/test/tap/lifecycle-path/node_modules/.bin"
    ].concat(PATH.split(pathSplit).map(function (p) {
      return p.replace(/\\/g, "/")
    }))
    t.same(actual, expect)
    t.end()
  })
})

test("clean", function (t) {
  rimraf.sync(link)
  t.end()
})

