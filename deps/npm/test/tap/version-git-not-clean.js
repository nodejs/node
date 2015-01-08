var common = require("../common-tap.js")
var test = require("tap").test
var npm = require("../../")
var osenv = require("osenv")
var path = require("path")
var fs = require("fs")
var rimraf = require("rimraf")
var mkdirp = require("mkdirp")
var which = require("which")
var spawn = require("child_process").spawn

var pkg = path.resolve(__dirname, "version-git-not-clean")
var cache = path.resolve(pkg, "cache")

test("npm version <semver> with working directory not clean", function (t) {
  setup()
  npm.load({ cache: cache, registry: common.registry, prefix: pkg }, function () {
    which("git", function (err, git) {
      t.ifError(err, "git found")

      function gitInit(_cb) {
        var child = spawn(git, ["init"])
        var out = ""
        child.stdout.on("data", function (d) {
          out += d.toString()
        })
        child.on("exit", function () {
          return _cb(out)
        })
      }

      function addPackageJSON(_cb) {
        var data = JSON.stringify({ name: "blah", version: "0.1.2" })
        fs.writeFile("package.json", data, function() {
          var child = spawn(git, ["add", "package.json"])
          child.on("exit", function () {
            var child2 = spawn(git, ["commit", "package.json", "-m", "init"])
            var out = ""
            child2.stdout.on("data", function (d) {
              out += d.toString()
            })
            child2.on("exit", function () {
              return _cb(out)
            })
          })
        })
      }

      gitInit(function() {
        addPackageJSON(function() {
          var data = JSON.stringify({ name: "blah", version: "0.1.3" })
          fs.writeFile("package.json", data, function() {
            npm.commands.version(["patch"], function (err) {
              if (!err) {
                t.fail("should fail on non-clean working directory")
              }
              else {
                t.equal(err.message, "Git working directory not clean.\nM package.json")
              }
              t.end()
            })
          })
        })
      })
    })
  })
})

test("cleanup", function (t) {
  // windows fix for locked files
  process.chdir(osenv.tmpdir())

  rimraf.sync(pkg)
  t.end()
})

function setup() {
  mkdirp.sync(pkg)
  mkdirp.sync(cache)
  process.chdir(pkg)
}
