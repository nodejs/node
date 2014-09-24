var tap = require("tap")
var crypto = require("crypto")
var fs = require("fs")

var server = require("./lib/server.js")
var common = require("./lib/common.js")

var nerfed = "//localhost:" + server.port + "/:"

var configuration = {}
configuration[nerfed + "username"]  = "username"
configuration[nerfed + "_password"] = new Buffer("%1234@asdf%").toString("base64")
configuration[nerfed + "email"]     = "ogd@aoaioxxysz.net"

var client = common.freshClient(configuration)

var _auth = new Buffer("username:%1234@asdf%").toString("base64")

tap.test("publish", function (t) {
  // not really a tarball, but doesn't matter
  var tarball = require.resolve("../package.json")
  var pd = fs.readFileSync(tarball, "base64")
  var pkg = require("../package.json")
  pkg.name = "@npm/npm-registry-client"

  server.expect("/@npm%2fnpm-registry-client", function (req, res) {
    t.equal(req.method, "PUT")
    t.equal(req.headers.authorization, "Basic " + _auth)

    var b = ""
    req.setEncoding("utf8")
    req.on("data", function (d) {
      b += d
    })

    req.on("end", function () {
      var o = JSON.parse(b)
      t.equal(o._id, "@npm/npm-registry-client")
      t.equal(o["dist-tags"].latest, pkg.version)
      t.has(o.versions[pkg.version], pkg)
      t.same(o.maintainers, [ { name: "username", email: "ogd@aoaioxxysz.net" } ])
      t.same(o.maintainers, o.versions[pkg.version].maintainers)
      var att = o._attachments[ pkg.name + "-" + pkg.version + ".tgz" ]
      t.same(att.data, pd)
      var hash = crypto.createHash("sha1").update(pd, "base64").digest("hex")
      t.equal(o.versions[pkg.version].dist.shasum, hash)
      res.statusCode = 201
      res.json({created:true})
    })
  })

  client.publish(common.registry, pkg, tarball, function (er, data) {
    if (er) throw er
    t.deepEqual(data, { created: true })
    t.end()
  })
})
