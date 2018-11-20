var cat = require("graceful-fs").writeFileSync
var exec = require("child_process").exec
var readdir = require("graceful-fs").readdirSync
var resolve = require("path").resolve

var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var test = require("tap").test
var tmpdir = require("osenv").tmpdir
var which = require("which")

var common = require("../common-tap.js")

var pkg = resolve(__dirname, "git-npmignore")
var dep = resolve(pkg, "deps", "gitch")
var packname = "gitch-1.0.0.tgz"
var packed = resolve(pkg, packname)
var modules = resolve(pkg, "node_modules")
var installed = resolve(modules, "gitch")
var expected = [
  "a.js",
  "package.json",
  ".npmignore"
].sort()

var EXEC_OPTS = {
  cwd : pkg
}

test("setup", function (t) {
  setup(function (er) {
    t.ifError(er, "setup ran OK")

    t.end()
  })
})

test("npm pack directly from directory", function (t) {
  packInstallTest(dep, t)
})

test("npm pack via git", function (t) {
  packInstallTest("git+file://"+dep, t)
})

test("cleanup", function (t) {
  cleanup()

  t.end()
})

function packInstallTest (spec, t) {
  common.npm(
    [
      "--loglevel", "silent",
      "pack", spec
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err,  "npm pack ran without error")
      t.notOk(code,   "npm pack exited cleanly")
      t.notOk(stderr, "npm pack ran silently")
      t.equal(stdout.trim(), packname, "got expected package name")

      common.npm(
        [
          "--loglevel", "silent",
          "install", packed
        ],
        EXEC_OPTS,
        function (err, code, stdout, stderr) {
          t.ifError(err,  "npm install ran without error")
          t.notOk(code,   "npm install exited cleanly")
          t.notOk(stderr, "npm install ran silently")

          var actual = readdir(installed).sort()
          t.same(actual, expected, "no unexpected files in packed directory")

          rimraf(packed, function () {
            t.end()
          })
        }
      )
    }
  )
}

var gitignore = "node_modules/\n"
var npmignore = "t.js\n"

var a = "console.log('hi');"
var t = "require('tap').test(function (t) { t.pass('I am a test!'); t.end(); });"
var fixture = {
  "name" : "gitch",
  "version" : "1.0.0",
  "private" : true,
  "main" : "a.js"
}

function cleanup () {
  process.chdir(tmpdir())
  rimraf.sync(pkg)
}

function setup (cb) {
  cleanup()

  mkdirp.sync(modules)
  mkdirp.sync(dep)

  process.chdir(dep)

  cat(resolve(dep, ".npmignore"), npmignore)
  cat(resolve(dep, ".gitignore"), gitignore)
  cat(resolve(dep, "a.js"), a)
  cat(resolve(dep, "t.js"), t)
  cat(resolve(dep, "package.json"), JSON.stringify(fixture))

  common.npm(
    [
      "--loglevel", "silent",
      "cache", "clean"
    ],
    EXEC_OPTS,
    function (er, code, _, stderr) {
      if (er) return cb(er)
      if (code) return cb(new Error("npm cache nonzero exit: "+code))
      if (stderr) return cb(new Error("npm cache clean error: "+stderr))

      which("git", function found (er, git) {
        if (er) return cb(er)

        exec(git+" init", init)

        function init (er, _, stderr) {
          if (er) return cb(er)
          if (stderr) return cb(new Error("git init error: "+stderr))

          exec(git+" config user.name 'Phantom Faker'", user)
        }

        function user (er, _, stderr) {
          if (er) return cb(er)
          if (stderr) return cb(new Error("git config error: "+stderr))

          exec(git+" config user.email nope@not.real", email)
        }

        function email (er, _, stderr) {
          if (er) return cb(er)
          if (stderr) return cb(new Error("git config error: "+stderr))

          exec(git+" add .", addAll)
        }

        function addAll (er, _, stderr) {
          if (er) return cb(er)
          if (stderr) return cb(new Error("git add . error: "+stderr))

          exec(git+" commit -m boot", commit)
        }

        function commit (er, _, stderr) {
          if (er) return cb(er)
          if (stderr) return cb(new Error("git commit error: "+stderr))

          cb()
        }
      })
    }
  )
}
