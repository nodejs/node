var test = require("tap").test
var rimraf = require("rimraf")
var path = require("path")
var osenv = require("osenv")
var mr = require("npm-registry-mock")
var spawn = require("child_process").spawn
var npm = require.resolve("../../bin/npm-cli.js")
var node = process.execPath
var pkg = path.resolve(__dirname, "url-dependencies")
var common = require("../common-tap")

var mockRoutes = {
  "get": {
    "/underscore/-/underscore-1.3.1.tgz": [200]
  }
}

test("url-dependencies: download first time", function(t) {
  cleanup()

  performInstall(function(output){
    if (!tarballWasFetched(output)){
      t.fail("Tarball was not fetched")
    } else {
      t.pass("Tarball was fetched")
    }
    t.end()
  })
})

test("url-dependencies: do not download subsequent times", function(t) {
  cleanup()

  performInstall(function(){
    performInstall(function(output){
      if (tarballWasFetched(output)){
        t.fail("Tarball was fetched second time around")
      } else {
        t.pass("Tarball was not fetched")
      }
      t.end()
    })
  })
})

function tarballWasFetched(output){
  return output.indexOf("http fetch GET " + common.registry + "/underscore/-/underscore-1.3.1.tgz") > -1
}

function performInstall (cb) {
  mr({port: common.port, mocks: mockRoutes}, function(s){
    var output = ""
      , child = spawn(node, [npm, "install"], {
          cwd: pkg,
          env: {
            "npm_config_registry": common.registry,
            "npm_config_cache_lock_stale": 1000,
            "npm_config_cache_lock_wait": 1000,
            "npm_config_loglevel": "http",
            HOME: process.env.HOME,
            Path: process.env.PATH,
            PATH: process.env.PATH
          }
        })

    child.stderr.on("data", function(data){
      output += data.toString()
    })
    child.on("close", function () {
      s.close()
      cb(output)
    })
  })
}

function cleanup() {
  // windows fix for locked files
  process.chdir(osenv.tmpdir())
  rimraf.sync(path.resolve(pkg, "node_modules"))
}
