var path = require("path")
var test = require("tap").test
var npm = require("../../lib/npm.js")
var requireInject = require("require-inject")

var realizePackageSpecifier = requireInject("realize-package-specifier", {
  "fs": {
    stat: function (file, cb) {
      process.nextTick(function () {
        switch (file) {
        case path.resolve("named"):
          cb(new Error("ENOENT"))
          break
        case path.resolve("file.tgz"):
          cb(null, { isDirectory: function () { return false } })
          break
        case path.resolve("dir-no-package"):
          cb(null, { isDirectory: function () { return true } })
          break
        case path.resolve("dir-no-package/package.json"):
          cb(new Error("ENOENT"))
          break
        case path.resolve("dir-with-package"):
          cb(null, { isDirectory: function () { return true } })
          break
        case path.resolve("dir-with-package/package.json"):
          cb(null, {})
          break
        case path.resolve(__dirname, "dir-with-package"):
          cb(null, { isDirectory: function () { return true } })
          break
        case path.join(__dirname, "dir-with-package", "package.json"):
          cb(null, {})
          break
        case path.resolve(__dirname, "file.tgz"):
          cb(null, { isDirectory: function () { return false } })
          break
        default:
          throw new Error("Unknown test file passed to stat: " + file)
        }
      })
    }
  }
})

npm.load({loglevel : "silent"}, function () {
  var cache = requireInject("../../lib/cache.js", {
    "realize-package-specifier":  realizePackageSpecifier,
    "../../lib/cache/add-named.js": function addNamed (name, version, data, cb) {
      cb(null, "addNamed")
    },
    "../../lib/cache/add-local.js": function addLocal (name, data, cb) {
      cb(null, "addLocal")
    }
  })

  test("npm install localdir fallback", function (t) {
    t.plan(12)
    cache.add("named", null, null, false, function (er, which) {
      t.ifError(er, "named was cached")
      t.is(which, "addNamed", "registry package name")
    })
    cache.add("file.tgz", null, null, false, function (er, which) {
      t.ifError(er, "file.tgz was cached")
      t.is(which, "addLocal", "local file")
    })
    cache.add("dir-no-package", null, null, false, function (er, which) {
      t.ifError(er, "local directory was cached")
      t.is(which, "addNamed", "local directory w/o package.json")
    })
    cache.add("dir-with-package", null, null, false, function (er, which) {
      t.ifError(er, "local directory with package was cached")
      t.is(which,"addLocal", "local directory with package.json")
    })
    cache.add("file:./dir-with-package", null, __dirname, false, function (er, which) {
      t.ifError(er, "local directory (as URI) with package was cached")
      t.is(which, "addLocal", "file: URI to local directory with package.json")
    })
    cache.add("file:./file.tgz", null, __dirname, false, function (er, which) {
      t.ifError(er, "local file (as URI) with package was cached")
      t.is(which, "addLocal", "file: URI to local file with package.json")
    })
  })
})
