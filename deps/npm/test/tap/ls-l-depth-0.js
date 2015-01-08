var cat = require("graceful-fs").writeFileSync
var resolve = require("path").resolve

var mkdirp = require("mkdirp")
var mr = require("npm-registry-mock")
var rimraf = require("rimraf")
var test = require("tap").test
var tmpdir = require("osenv").tmpdir

var common = require("../common-tap.js")

var pkg = resolve(__dirname, "ls-l-depth-0")
var dep = resolve(pkg, "deps", "glock")
var modules = resolve(pkg, "node_modules")

var expected =
  "\n" +
  "│ " + pkg + "\n" +
  "│ \n" +
  "└── glock@1.8.7\n" +
  "    an inexplicably hostile sample package\n" +
  "    https://github.com/npm/glo.ck\n" +
  "    https://glo.ck\n" +
  "\n"

var server

var EXEC_OPTS = {
  cwd : pkg
}

test("setup", function (t) {
  setup()
  mr(common.port, function (s) {
    server = s

    t.end()
  })
})

test("#6311: npm ll --depth=0 duplicates listing", function (t) {
  common.npm(
    [
      "--loglevel", "silent",
      "--registry", common.registry,
      "install", dep
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err,  "npm install ran without error")
      t.notOk(code,   "npm install exited cleanly")
      t.notOk(stderr, "npm install ran silently")
      t.equal(
        stdout.trim(),
        "glock@1.8.7 node_modules/glock\n└── underscore@1.5.1",
        "got expected install output"
      )

      common.npm(
        [
          "--loglevel", "silent",
          "ls", "--long",
          "--depth", "0"
        ],
        EXEC_OPTS,
        function (err, code, stdout, stderr) {
          t.ifError(err,  "npm ll ran without error")
          t.notOk(code,   "npm ll exited cleanly")
          t.notOk(stderr, "npm ll ran silently")
          t.equal(
            stdout,
            expected,
            "got expected package name"
          )

          t.end()
        }
      )
    }
  )
})

test("cleanup", function (t) {
  cleanup()
  server.close()

  t.end()
})

var fixture = {
  "name" : "glock",
  "version" : "1.8.7",
  "private" : true,
  "description" : "an inexplicably hostile sample package",
  "homepage" : "https://glo.ck",
  "repository" : "https://github.com/npm/glo.ck",
  "dependencies" : {
    "underscore" : "1.5.1"
  }
}

function cleanup () {
  process.chdir(tmpdir())
  rimraf.sync(pkg)
}

function setup () {
  cleanup()

  mkdirp.sync(modules)
  mkdirp.sync(dep)

  cat(resolve(dep, "package.json"), JSON.stringify(fixture))
}
