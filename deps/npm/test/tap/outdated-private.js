var common = require("../common-tap.js")
var test = require("tap").test
var npm = require("../../")
var rimraf = require("rimraf")
var path = require("path")
var mr = require("npm-registry-mock")
var osenv = require("osenv")
var mkdirp = require("mkdirp")
var fs = require("graceful-fs")

var pkg = path.resolve(__dirname, "outdated-private")
var pkgLocalPrivate = path.resolve(pkg, "local-private")
var pkgScopedLocalPrivate = path.resolve(pkg, "another-local-private")
var pkgLocalUnderscore = path.resolve(pkg, "underscore")

test("setup", function (t) {
  bootstrap()
  t.end()
})

test("outdated ignores private modules", function (t) {
  t.plan(3)
  process.chdir(pkg)
  mr({ port : common.port }, function (err, s) {
    npm.load(
      {
        loglevel  : "silent",
        parseable : true,
        registry  : common.registry
      },
      function () {
        npm.install(".", function (err) {
          t.ifError(err, "install success")
          npm.outdated(function (er, d) {
            t.ifError(er, "outdated success")
            t.deepEqual(d, [[
              path.resolve(__dirname, "outdated-private"),
              "underscore",
              "1.3.1",
              "1.3.1",
              "1.5.1",
              "file:underscore"
            ]])
            s.close()
          })
        })
      }
    )
  })
})

test("cleanup", function (t) {
  cleanup()
  t.end()
})

var pjParent = JSON.stringify({
  name         : "outdated-private",
  version      : "1.0.0",
  dependencies : {
    "local-private" : "file:local-private",
    "@scoped/another-local-private" : "file:another-local-private",
    "underscore" : "file:underscore"
  }
}, null, 2) + "\n"

var pjLocalPrivate = JSON.stringify({
  name         : "local-private",
  version      : "1.0.0",
  private      : true
}, null, 2) + "\n"

var pjScopedLocalPrivate = JSON.stringify({
  name         : "@scoped/another-local-private",
  version      : "1.0.0",
  private      : true
}, null, 2) + "\n"

var pjLocalUnderscore = JSON.stringify({
  name         : "underscore",
  version      : "1.3.1"
}, null, 2) + "\n"

function bootstrap () {
  mkdirp.sync(pkg)
  fs.writeFileSync(path.resolve(pkg, "package.json"), pjParent)

  mkdirp.sync(pkgLocalPrivate)
  fs.writeFileSync(path.resolve(pkgLocalPrivate, "package.json"), pjLocalPrivate)

  mkdirp.sync(pkgScopedLocalPrivate)
  fs.writeFileSync(path.resolve(pkgScopedLocalPrivate, "package.json"), pjScopedLocalPrivate)

  mkdirp.sync(pkgLocalUnderscore)
  fs.writeFileSync(path.resolve(pkgLocalUnderscore, "package.json"), pjLocalUnderscore)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
