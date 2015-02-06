var fs = require("fs")
var path = require("path")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")

var test = require("tap").test
var common = require("../common-tap.js")

var pkg = path.resolve(__dirname, "access")
var server

var scoped = {
  name : "@scoped/pkg",
  version : "1.1.1"
}

var body = {
  access : "public"
}

function mocks (server) {
  server.post("/-/package/@scoped%2fpkg/access", JSON.stringify(body))
    .reply(200, { "access" : "public" })
  server.post("/-/package/@scoped%2fanother/access", JSON.stringify(body))
    .reply(200, { "access" : "public" })
}

test("setup", function (t) {
  mkdirp(pkg, function (er) {
    t.ifError(er, pkg + " made successfully")

    mr({port : common.port, plugin : mocks}, function (err, s) {
      server = s

      fs.writeFile(
        path.join(pkg, "package.json"),
        JSON.stringify(scoped),
        function (er) {
          t.ifError(er, "wrote package.json")
          t.end()
        }
      )
    })
  })
})

test("npm access on current package", function (t) {
  common.npm(
    [
      "access",
      "public",
      "--registry", common.registry,
      "--loglevel", "silent"
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, "npm access")
      t.notOk(code, "exited OK")
      t.notOk(stderr, "no error output")

      t.end()
    }
  )
})

test("npm access on named package", function (t) {
  common.npm(
    [
      "access",
      "public", "@scoped/another",
      "--registry", common.registry,
      "--loglevel", "silent"
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, "npm access")
      t.notOk(code, "exited OK")
      t.notOk(stderr, "no error output")

      t.end()
    }
  )
})

test("cleanup", function (t) {
  t.pass("cleaned up")
  rimraf.sync(pkg)
  server.close()
  t.end()
})
