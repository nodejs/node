var common = require("../common-tap")
var test = require("tap").test
var fs = require("fs")

test("cleanup", function (t) {
  var res = common.deleteNpmCacheRecursivelySync()
  t.equal(res, 0, "Deleted test npm cache successfully")

  // ensure cache is clean
  fs.readdir(common.npm_config_cache, function (err) {
    t.ok(err, "error expected")
    t.equal(err.code, "ENOENT", "npm cache directory no longer exists")
    t.end()
  })
})
