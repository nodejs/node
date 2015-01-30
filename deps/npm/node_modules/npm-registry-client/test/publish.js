var test = require("tap").test
var crypto = require("crypto")
var fs = require("fs")

var server = require("./lib/server.js")
var common = require("./lib/common.js")
var client = common.freshClient()

function nop () {}

var URI       = "http://localhost:1337/"
var USERNAME  = "username"
var PASSWORD  = "%1234@asdf%"
var EMAIL     = "i@izs.me"
var METADATA  = require("../package.json")
var ACCESS    = "public"
// not really a tarball, but doesn't matter
var BODY_PATH = require.resolve("../package.json")
var BODY      = fs.createReadStream(BODY_PATH, "base64")
var AUTH      = {
  username : USERNAME,
  password : PASSWORD,
  email    : EMAIL
}
var PARAMS  = {
  metadata : METADATA,
  access   : ACCESS,
  body     : BODY,
  auth     : AUTH
}

test("publish call contract", function (t) {
  t.throws(function () {
    client.publish(undefined, PARAMS, nop)
  }, "requires a URI")

  t.throws(function () {
    client.publish([], PARAMS, nop)
  }, "requires URI to be a string")

  t.throws(function () {
    client.publish(URI, undefined, nop)
  }, "requires params object")

  t.throws(function () {
    client.publish(URI, "", nop)
  }, "params must be object")

  t.throws(function () {
    client.publish(URI, PARAMS, undefined)
  }, "requires callback")

  t.throws(function () {
    client.publish(URI, PARAMS, "callback")
  }, "callback must be function")

  t.throws(
    function () {
      var params = {
        access : ACCESS,
        body : BODY,
        auth : AUTH
      }
      client.publish(URI, params, nop)
    },
    { name : "AssertionError", message : "must pass package metadata to publish" },
    "params must include metadata for package"
  )

  t.throws(
    function () {
      var params = {
        metadata : METADATA,
        body : BODY,
        auth : AUTH
      }
      client.publish(URI, params, nop)
    },
    { name : "AssertionError", message : "must pass access for package" },
    "params must include access for package"
  )

  t.throws(
    function () {
      var params = {
        metadata : METADATA,
        access : ACCESS,
        auth : AUTH
      }
      client.publish(URI, params, nop)
    },
    { name : "AssertionError", message : "must pass package body to publish" },
    "params must include body of package to publish"
  )

  t.throws(
    function () {
      var params = {
        metadata : METADATA,
        access : ACCESS,
        body : BODY
      }
      client.publish(URI, params, nop)
    },
    { name : "AssertionError", message : "must pass auth to publish" },
    "params must include auth"
  )

  t.throws(
    function () {
      var params = {
        metadata : -1,
        access : ACCESS,
        body : BODY,
        auth : AUTH
      }
      client.publish(URI, params, nop)
    },
    { name : "AssertionError", message : "must pass package metadata to publish" },
    "metadata must be object"
  )

  t.throws(
    function () {
      var params = {
        metadata : METADATA,
        access : "hamchunx",
        body : BODY,
        auth : AUTH
      }
      client.publish(URI, params, nop)
    },
    {
      name    : "AssertionError",
      message : "access level must be either 'public' or 'restricted'"
    },
    "access level must be 'public' or 'restricted'"
  )

  t.throws(
    function () {
      var params = {
        metadata : METADATA,
        access : ACCESS,
        body : -1,
        auth : AUTH
      }
      client.publish(URI, params, nop)
    },
    {
      name    : "AssertionError",
      message : "package body passed to publish must be a stream"
    },
    "body must be a Stream"
  )

  t.test("malformed semver in publish", function (t) {
    var metadata = JSON.parse(JSON.stringify(METADATA))
    metadata.version = "%!@#$"
    var params = {
      metadata : metadata,
      access : ACCESS,
      message : BODY,
      auth : AUTH
    }
    client.publish(URI, params, function (err) {
      t.equal(
        err && err.message,
        "invalid semver: %!@#$",
        "got expected semver validation failure"
      )
      t.end()
    })
  })

  t.end()
})

test("publish", function (t) {
  var pd = fs.readFileSync(BODY_PATH, "base64")

  server.expect("/npm-registry-client", function (req, res) {
    t.equal(req.method, "PUT")
    var b = ""
    req.setEncoding("utf8")
    req.on("data", function (d) {
      b += d
    })

    req.on("end", function () {
      var o = JSON.parse(b)
      t.equal(o._id, "npm-registry-client")
      t.equal(o["dist-tags"].latest, METADATA.version)
      t.equal(o.access, ACCESS)
      t.has(o.versions[METADATA.version], METADATA)
      t.same(o.maintainers, [{ name : "username", email : "i@izs.me" }])
      t.same(o.maintainers, o.versions[METADATA.version].maintainers)

      var att = o._attachments[METADATA.name+"-"+METADATA.version+".tgz"]
      t.same(att.data, pd)

      var hash = crypto.createHash("sha1").update(pd, "base64").digest("hex")
      t.equal(o.versions[METADATA.version].dist.shasum, hash)

      res.statusCode = 201
      res.json({ created : true })
    })
  })

  client.publish(URI, PARAMS, function (er, data) {
    if (er) throw er

    t.deepEqual(data, { created : true })
    t.end()
  })
})
