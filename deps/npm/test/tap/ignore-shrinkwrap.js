var common = require("../common-tap.js")
var test = require("tap").test
var pkg = './ignore-shrinkwrap'

var mr = require("npm-registry-mock")

var child
var spawn = require("child_process").spawn
var npm = require.resolve("../../bin/npm-cli.js")
var node = process.execPath

var customMocks = {
  "get": {
    "/package.js": [200, {"ente" : true}],
    "/shrinkwrap.js": [200, {"ente" : true}]
  }
}

test("ignore-shrinkwrap: using the option", function (t) {
  mr({port: common.port, mocks: customMocks}, function (s) {
    s._server.on("request", function (req, res) {
      switch (req.url) {
        case "/shrinkwrap.js":
          t.fail()
          break
        case "/package.js":
          t.pass("package.json used")
      }
    })
    var child = createChild(true)
    child.on("close", function (m) {
      s.close()
      t.end()
    })
  })
})

test("ignore-shrinkwrap: NOT using the option", function (t) {
  mr({port: common.port, mocks: customMocks}, function (s) {
    s._server.on("request", function (req, res) {
      switch (req.url) {
        case "/shrinkwrap.js":
          t.pass("shrinkwrap used")
          break
        case "/package.js":
          t.fail()
      }
    })
    var child = createChild(false)
    child.on("close", function (m) {
      s.close()
      t.end()
    })
  })
})

function createChild (ignoreShrinkwrap) {
  var args
  if (ignoreShrinkwrap) {
    args = [npm, "install", "--no-shrinkwrap"]
  } else {
    args = [npm, "install"]
  }

  return spawn(node, args, {
    cwd: pkg,
    env: {
      npm_config_registry: common.registry,
      npm_config_cache_lock_stale: 1000,
      npm_config_cache_lock_wait: 1000,
      HOME: process.env.HOME,
      Path: process.env.PATH,
      PATH: process.env.PATH
    }
  })

}
