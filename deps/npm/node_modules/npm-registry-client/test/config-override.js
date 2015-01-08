var test = require("tap").test

require("./lib/server.js").close()
var common = require("./lib/common.js")
var config = {
  proxy : {
    http : "http://proxy.npm:8088/",
    https : "https://proxy.npm:8043/",
    localAddress : "localhost.localdomain"
  },
  ssl : {
    ca : "not including a PEM",
    certificate : "still not including a PEM",
    key : "nope",
    strict : false
  },
  retry : {
    count : 1,
    factor : 9001,
    minTimeout : -1,
    maxTimeout : Infinity
  },
  userAgent : "npm-awesome/4 (Mozilla 5.0)",
  log : { fake : function () {} },
  defaultTag : "next",
  couchToken : { object : true },
  sessionToken : "hamchunx"
}

test("config defaults", function (t) {
  var client = common.freshClient(config)

  var proxy = client.config.proxy
  t.equal(proxy.http, "http://proxy.npm:8088/")
  t.equal(proxy.https, "https://proxy.npm:8043/")
  t.equal(proxy.localAddress, "localhost.localdomain")

  var ssl = client.config.ssl
  t.equal(ssl.ca, "not including a PEM")
  t.equal(ssl.certificate, "still not including a PEM")
  t.equal(ssl.key, "nope")
  t.equal(ssl.strict, false)

  var retry = client.config.retry
  t.equal(retry.count, 1)
  t.equal(retry.factor, 9001)
  t.equal(retry.minTimeout, -1)
  t.equal(retry.maxTimeout, Infinity)

  t.equal(client.config.userAgent, "npm-awesome/4 (Mozilla 5.0)")
  t.ok(client.log.fake)
  t.equal(client.config.defaultTag, "next")
  t.ok(client.config.couchToken.object)
  t.equal(client.config.sessionToken, "hamchunx")

  t.end()
})
