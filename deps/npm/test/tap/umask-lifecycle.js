var path = require("path")
var test = require("tap").test
var fs = require("fs")
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")
var common = require("../common-tap.js")

var pkg = path.resolve(__dirname, "umask-lifecycle")
var pj = JSON.stringify({
  name:"x",
  version: "1.2.3",
  scripts: { umask: "$npm_execpath config get umask && echo \"$npm_config_umask\" && node -p 'process.umask()'" }
}, null, 2) + "\n"

var expected = [
  "",
  "> x@1.2.3 umask "+path.join(__dirname, "umask-lifecycle"),
  "> $npm_execpath config get umask && echo \"$npm_config_umask\" && node -p 'process.umask()'",
  "",
  "0022",
  "0022",
  "18",
  ""
].join("\n")

test("setup", function (t) {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  fs.writeFileSync(pkg + "/package.json", pj)
  t.end()
})

test("umask script", function (t) {
  common.npm(["run", "umask"], {
    umask: 0022,
    cwd: pkg,
    env: {
      PATH: process.env.PATH,
      Path: process.env.Path,
      "npm_config_loglevel": "warn"
    }
  }, function (er, code, sout, serr) {
    t.equal(sout, expected)
    t.equal(serr, "")
    t.end()
  })
})

test("clean", function (t) {
  rimraf.sync(pkg)
  t.end()
})
