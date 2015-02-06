var test = require("tap").test

// var server = require("./lib/server.js")
var Client = require("../")

test("defaulted initialization", function (t) {
  var client = new Client()
  var options = client.initialize(
    "http://localhost:1337/",
    "GET",
    "application/json",
    {}
  )

  t.equal(options.url, "http://localhost:1337/", "URLs match")
  t.equal(options.method, "GET", "methods match")
  t.equal(options.proxy, undefined, "proxy won't overwrite environment")
  t.equal(options.localAddress, undefined, "localAddress has no default value")
  t.equal(options.strictSSL, true, "SSL is strict by default")

  t.equal(options.headers.accept, "application/json", "accept header set")
  t.equal(
    options.headers.version,
    require("../package.json").version,
    "npm-registry-client version is present in headers"
  )
  t.ok(options.headers["npm-session"], "request ID generated")
  t.ok(options.headers["user-agent"], "user-agent preset")

  var HttpAgent = require("http").Agent
  t.ok(options.agent instanceof HttpAgent, "got an HTTP agent for an HTTP URL")

  t.end()
})

test("referer set on client", function (t) {
  var client = new Client()
  client.refer = "xtestx"
  var options = client.initialize(
    "http://localhost:1337/",
    "GET",
    "application/json",
    {}
  )

  t.equal(options.headers.referer, "xtestx", "referer header set")

  t.end()
})

test("initializing with proxy explicitly disabled", function (t) {
  var client = new Client({ proxy : { http : false }})
  var options = client.initialize(
    "http://localhost:1337/",
    "GET",
    "application/json",
    {}
  )
  t.ok("proxy" in options, "proxy overridden by explicitly setting to false")
  t.equal(options.proxy, null, "request will override proxy when empty proxy passed in")
  t.end()
})

test("initializing with proxy undefined", function (t) {
  var client = new Client({ proxy : { http : undefined }})
  var options = client.initialize(
    "http://localhost:1337/",
    "GET",
    "application/json",
    {}
  )
  t.notOk("proxy" in options, "proxy can be read from env.PROXY by request")
  t.end()
})
