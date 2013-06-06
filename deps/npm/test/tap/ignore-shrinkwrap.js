var test = require("tap").test
var npm = require("../../")
var pkg = './ignore-shrinkwrap'
var http = require("http")


var server, child
var spawn = require("child_process").spawn
var npm = require.resolve("../../bin/npm-cli.js")
var node = process.execPath

test("ignore-shrinkwrap: using the option", function(t) {
  t.plan(1)
  server = http.createServer(function (req, res) {
    res.setHeader("content-type", "application/javascript")
    switch (req.url) {
      case "/shrinkwrap.js":
        t.fail()
        break
      case "/package.js":
        t.pass("package.json used")

    }
    t.end()
    this.close()
    child.kill()
    res.statusCode = 500
    res.end('{"error":"Rocko Artischocko - oh oh oh oh!"}')
  })
  server.listen(1337, function() {
    child = createChild(true)
  })
})

test("ignore-shrinkwrap: NOT using the option", function(t) {
  t.plan(1)
  server = http.createServer(function (req, res) {
    res.setHeader("content-type", "application/javascript")
    switch (req.url) {
      case "/shrinkwrap.js":
        t.pass("shrinkwrap used")
        break
      case "/package.js":
        t.fail()

    }
    t.end()
    this.close()
    child.kill()
    res.statusCode = 500
    res.end('{"error":"Rocko Artischocko - oh oh oh oh!"}')
  })
  server.listen(1337, function() {
    child = createChild(false)
  })
})


function createChild (ignoreShrinkwrap) {
  var args
  if (ignoreShrinkwrap) {
    args = [npm, "install", "--no-shrinkwrap"]
  } else {
    args = [npm, "install"]
  }

  console.log(args)

  return spawn(node, args, {
    cwd: pkg,
    env: {
      npm_config_cache_lock_stale: 1000,
      npm_config_cache_lock_wait: 1000,
      HOME: process.env.HOME,
      Path: process.env.PATH,
      PATH: process.env.PATH
    }
  })

}
