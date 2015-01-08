var resolve = require("path").resolve
var writeFileSync = require("graceful-fs").writeFileSync

var mkdirp = require("mkdirp")
var mr = require("npm-registry-mock")
var osenv = require("osenv")
var rimraf = require("rimraf")
var test = require("tap").test

var common = require("../common-tap.js")
var toNerfDart = require("../../lib/config/nerf-dart.js")

var pkg = resolve(__dirname, "shrinkwrap-scoped-auth")
var outfile = resolve(pkg, "_npmrc")
var modules = resolve(pkg, "node_modules")
var tarballPath = "/scoped-underscore/-/scoped-underscore-1.3.1.tgz"
var tarballURL = common.registry + tarballPath
var tarball = resolve(__dirname, "../fixtures/scoped-underscore-1.3.1.tgz")

var server

var EXEC_OPTS = {
  cwd : pkg
}

function mocks (server) {
  var auth = "Bearer 0xabad1dea"
  server.get(tarballPath, { authorization : auth }).replyWithFile(200, tarball)
  server.get(tarballPath).reply(401, {
    error  : "unauthorized",
    reason : "You are not authorized to access this db."
  })
}

test("setup", function (t) {
  mr({ port : common.port, mocks : mocks }, function (s) {
    server = s
    t.ok(s, "set up mock registry")
    setup()
    t.end()
  })
})

test("authed npm install with shrinkwrapped scoped package", function (t) {
  common.npm(
    [
      "install",
      "--loglevel", "silent",
      "--fetch-retries", 0,
      "--userconfig", outfile
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err, "test runner executed without error")
      t.equal(code, 0, "npm install exited OK")
      t.notOk(stderr, "no output on stderr")
      t.equal(
        stdout,
        "@scoped/underscore@1.3.1 node_modules/@scoped/underscore\n",
        "module installed where expected"
      )

      t.end()
    }
  )
})

test("cleanup", function (t) {
  server.close()
  cleanup()
  t.end()
})

var contents = "@scoped:registry="+common.registry+"\n" +
               toNerfDart(common.registry)+":_authToken=0xabad1dea\n"

var json = {
  name : "test-package-install",
  version : "1.0.0"
}

var shrinkwrap = {
  name : "test-package-install",
  version : "1.0.0",
  dependencies : {
    "@scoped/underscore" : {
      resolved : tarballURL,
      from     : ">=1.3.1 <2",
      version  : "1.3.1"
    }
  }
}

function setup () {
  cleanup()
  mkdirp.sync(modules)
  writeFileSync(resolve(pkg, "package.json"), JSON.stringify(json, null, 2)+"\n")
  writeFileSync(outfile, contents)
  writeFileSync(
    resolve(pkg, "npm-shrinkwrap.json"),
    JSON.stringify(shrinkwrap, null, 2)+"\n"
  )
}

function cleanup() {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
