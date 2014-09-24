var path = require("path")
var test = require("tap").test
var npm = require("../../lib/npm.js")
var requireInject = require("require-inject")

npm.load({loglevel : "silent"}, function () {
  var resolved = path.resolve(__dirname, "dir-with-package")
  var resolvedPackage = path.join(resolved, "package.json")

  var cache = requireInject("../../lib/cache.js", {
    "graceful-fs": {
      stat: function (file, cb) {
        process.nextTick(function () {
          switch (file) {
          case "named":
            cb(new Error("ENOENT"))
            break
          case "file.tgz":
            cb(null, { isDirectory: function () { return false } })
            break
          case "dir-no-package":
            cb(null, { isDirectory: function () { return true } })
            break
          case "dir-no-package/package.json":
            cb(new Error("ENOENT"))
            break
          case "dir-with-package":
            cb(null, { isDirectory: function () { return true } })
            break
          case "dir-with-package/package.json":
            cb(null, {})
            break
          case resolved:
            cb(null, { isDirectory: function () { return true } })
            break
          case resolvedPackage:
            cb(null, {})
            break
          default:
            throw new Error("Unknown test file passed to stat: " + file)
          }
        })
      }
    },
    "../../lib/cache/add-named.js": function addNamed (name, version, data, cb) {
      cb(null, "addNamed")
    },
    "../../lib/cache/add-local.js": function addLocal (name, data, cb) {
      cb(null, "addLocal")
    }
  })

  test("npm install localdir fallback", function (t) {
    t.plan(10)
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
  })
})
