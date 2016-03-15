var fs = require("fs")
var resolve = require("path").resolve

var osenv = require("osenv")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var test = require("tap").test

var common = require("../common-tap.js")

var pkg = resolve(__dirname, "install-man")
var target = resolve(__dirname, "install-man-target")

var EXEC_OPTS = {
  cwd: target
}

var json = {
  name    : "install-man",
  version : "1.2.3",
  man     : [ "./install-man.1" ]
}

test("setup", function (t) {
  setup()
  t.pass("setup ran")
  t.end()
})

test("install man page", function (t) {
  common.npm(
    [
      "install",
      "--prefix", target,
      "--global",
      pkg
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err, "npm command ran from test")
      t.equals(code, 0, "install exited with success (0)")
      t.ok(stdout, "output indicating success")
      t.ok(
        fs.existsSync(resolve(target, "share", "man", "man1", "install-man.1")),
        "man page link was created"
      )

      t.end()
    }
  )
})

test("clean", function (t) {
  cleanup()
  t.pass("cleaned up")
  t.end()
})

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  // make sure it installs locally
  mkdirp.sync(resolve(target, "node_modules"))
  fs.writeFileSync(
    resolve(pkg, "package.json"),
    JSON.stringify(json, null, 2)+"\n"
  )
  fs.writeFileSync(resolve(pkg, "install-man.1"), "THIS IS A MANPAGE\n")
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
  rimraf.sync(target)
}
