var fs = require("fs")
var path = require("path")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")

var test = require("tap").test
var common = require("../common-tap.js")

var pkg = path.resolve(__dirname, "dist-tag")
var server

var scoped = {
  name : "@scoped/pkg",
  version : "1.1.1"
}

function mocks (server) {
  // ls current package
  server.get("/-/package/@scoped%2fpkg/dist-tags")
    .reply(200, { latest : "1.0.0", a : "0.0.1", b : "0.5.0" })

  // ls named package
  server.get("/-/package/@scoped%2fanother/dist-tags")
    .reply(200, { latest : "2.0.0", a : "0.0.2", b : "0.6.0" })

  // add c
  server.get("/-/package/@scoped%2fanother/dist-tags")
    .reply(200, { latest : "2.0.0", a : "0.0.2", b : "0.6.0" })
  server.put("/-/package/@scoped%2fanother/dist-tags/c", "\"7.7.7\"")
    .reply(200, { latest : "7.7.7", a : "0.0.2", b : "0.6.0", c : "7.7.7" })

  // set same version
  server.get("/-/package/@scoped%2fanother/dist-tags")
    .reply(200, { latest : "2.0.0", b : "0.6.0" })

  // rm
  server.get("/-/package/@scoped%2fanother/dist-tags")
    .reply(200, { latest : "2.0.0", a : "0.0.2", b : "0.6.0", c : "7.7.7" })
  server.delete("/-/package/@scoped%2fanother/dist-tags/c")
    .reply(200, { c : "7.7.7" })

  // rm
  server.get("/-/package/@scoped%2fanother/dist-tags")
    .reply(200, { latest : "4.0.0" })
}

test("setup", function (t) {
  mkdirp(pkg, function (er) {
    t.ifError(er, pkg + " made successfully")

    mr({port : common.port, mocks : mocks}, function (s) {
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

test("npm dist-tags ls in current package", function (t) {
  common.npm(
    [
      "dist-tags", "ls",
      "--registry", common.registry,
      "--loglevel", "silent"
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, "npm access")
      t.notOk(code, "exited OK")
      t.notOk(stderr, "no error output")
      t.equal(stdout, "a: 0.0.1\nb: 0.5.0\nlatest: 1.0.0\n")

      t.end()
    }
  )
})

test("npm dist-tags ls on named package", function (t) {
  common.npm(
    [
      "dist-tags",
      "ls", "@scoped/another",
      "--registry", common.registry,
      "--loglevel", "silent"
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, "npm access")
      t.notOk(code, "exited OK")
      t.notOk(stderr, "no error output")
      t.equal(stdout, "a: 0.0.2\nb: 0.6.0\nlatest: 2.0.0\n")

      t.end()
    }
  )
})

test("npm dist-tags add @scoped/another@7.7.7 c", function (t) {
  common.npm(
    [
      "dist-tags",
      "add", "@scoped/another@7.7.7", "c",
      "--registry", common.registry,
      "--loglevel", "silent"
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, "npm access")
      t.notOk(code, "exited OK")
      t.notOk(stderr, "no error output")
      t.equal(stdout, "+c: @scoped/another@7.7.7\n")

      t.end()
    }
  )
})

test("npm dist-tags set same version", function (t) {
  common.npm(
    [
      "dist-tag",
      "set", "@scoped/another@0.6.0", "b",
      "--registry", common.registry,
      "--loglevel", "warn"
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, "npm access")
      t.notOk(code, "exited OK")
      t.equal(
        stderr,
        "npm WARN dist-tag add b is already set to version 0.6.0\n",
        "warned about setting same version"
      )
      t.notOk(stdout, "only expecting warning message")

      t.end()
    }
  )
})

test("npm dist-tags rm @scoped/another c", function (t) {
  common.npm(
    [
      "dist-tags",
      "rm", "@scoped/another", "c",
      "--registry", common.registry,
      "--loglevel", "silent"
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, "npm access")
      t.notOk(code, "exited OK")
      t.notOk(stderr, "no error output")
      t.equal(stdout, "-c: @scoped/another@7.7.7\n")

      t.end()
    }
  )
})

test("npm dist-tags rm @scoped/another nonexistent", function (t) {
  common.npm(
    [
      "dist-tags",
      "rm", "@scoped/another", "nonexistent",
      "--registry", common.registry,
      "--loglevel", "silent"
    ],
    { cwd : pkg },
    function (er, code, stdout, stderr) {
      t.ifError(er, "npm dist-tag")
      t.ok(code, "expecting nonzero exit code")
      t.notOk(stderr, "no error output")
      t.notOk(stdout, "not expecting output")

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
