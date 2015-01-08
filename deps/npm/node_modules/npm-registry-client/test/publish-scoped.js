var tap = require("tap")
var crypto = require("crypto")
var fs = require("fs")

var server = require("./lib/server.js")
var common = require("./lib/common.js")

var auth = {
  username : "username",
  password : "%1234@asdf%",
  email : "ogd@aoaioxxysz.net"
}

var client = common.freshClient()

var _auth = new Buffer("username:%1234@asdf%").toString("base64")

tap.test("publish", function (t) {
  // not really a tarball, but doesn't matter
  var bodyPath = require.resolve("../package.json")
  var tarball = fs.createReadStream(bodyPath, "base64")
  var pd = fs.readFileSync(bodyPath, "base64")
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

  var params = {
    metadata : pkg,
    body : tarball,
    auth : auth
  }
  client.publish(common.registry, params, function (er, data) {
    if (er) throw er
    t.deepEqual(data, { created: true })
    t.end()
  })
})
