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

test("npm change access on unscoped package", function (t) {
  common.npm(
    [
      "access",
      "restricted", "yargs",
      "--registry", common.registry
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ok(code, 'exited with Error')
      t.ok(stderr.match(/you can't change the access level of unscoped packages/))
      t.end()
    }
  )
})

test('npm access add', function (t) {
  common.npm(
    [
      "access",
      "add", "@scoped/another",
      "--registry", common.registry
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ok(code, 'exited with Error')
      t.ok(stderr.match(/npm access add isn't implemented yet!/))
      t.end()
    }
  )
})

test('npm access rm', function (t) {
  common.npm(
    [
      "access",
      "rm", "@scoped/another",
      "--registry", common.registry
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ok(code, 'exited with Error')
      t.ok(stderr.match(/npm access rm isn't implemented yet!/))
      t.end()
    }
  )
})

test('npm access ls', function (t) {
  common.npm(
    [
      "access",
      "ls", "@scoped/another",
      "--registry", common.registry
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ok(code, 'exited with Error')
      t.ok(stderr.match(/npm access ls isn't implemented yet!/))
      t.end()
    }
  )
})

test('npm access edit', function (t) {
  common.npm(
    [
      "access",
      "edit", "@scoped/another",
      "--registry", common.registry
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ok(code, 'exited with Error')
      t.ok(stderr.match(/npm access edit isn't implemented yet!/))
      t.end()
    }
  )
})

test('npm access blerg', function (t) {
  common.npm(
    [
      "access",
      "blerg", "@scoped/another",
      "--registry", common.registry
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ok(code, 'exited with Error')
      t.ok(stderr.match(/Usage:/))
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
