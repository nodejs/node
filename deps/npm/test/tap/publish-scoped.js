var fs = require("fs")
var path = require("path")

var test = require("tap").test
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var nock = require("nock")

var npm = require("../../")
var common = require("../common-tap.js")

var pkg = path.join(__dirname, "prepublish_package")

test("setup", function (t) {
  mkdirp(path.join(pkg, "cache"), next)

  function next () {
    process.chdir(pkg)
    fs.writeFile(
      path.join(pkg, "package.json"),
      JSON.stringify({
        name: "@bigco/publish-organized",
        version: "1.2.5"
      }),
      "ascii",
      function (er) {
        t.ifError(er)

        t.pass("setup done")
        t.end()
      }
    )
  }
})

test("npm publish should honor scoping", function (t) {
  var put = nock(common.registry).put("/@bigco%2fpublish-organized").reply(201, {ok: true})

  var configuration = {
    cache    : path.join(pkg, "cache"),
    loglevel : "silent",
    registry : "http://nonexistent.lvh.me",
    "//localhost:1337/:username" : "username",
    "//localhost:1337/:_password" : new Buffer("password").toString("base64"),
    "//localhost:1337/:email" : "ogd@aoaioxxysz.net"
  }

  npm.load(configuration, onload)

  function onload (er) {
    t.ifError(er, "npm bootstrapped successfully")

    npm.config.set("@bigco:registry", common.registry)
    npm.commands.publish([], false, function (er) {
      t.ifError(er, "published without error")

      put.done()

      t.end()
    })
  }
})

test("cleanup", function(t) {
  process.chdir(__dirname)
  rimraf(pkg, function (er) {
    t.ifError(er)

    t.end()
  })
})
