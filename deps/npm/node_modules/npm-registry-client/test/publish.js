var tap = require('tap')
var crypto = require('crypto')
var server = require('./fixtures/server.js')
var RC = require('../')
var client = new RC(
  { cache: __dirname + '/fixtures/cache'
  , registry: 'http://localhost:' + server.port
  , username: "username"
  , password: "password"
  , email: "i@izs.me"
  , _auth: new Buffer("username:password").toString('base64')
  , "always-auth": true
  })

var fs = require("fs")

tap.test("publish", function (t) {
  server.expect("/npm-registry-client", function (req, res) {
    t.equal(req.method, "PUT")
    var b = ""
    req.setEncoding('utf8')
    req.on("data", function (d) {
      b += d
    })

    req.on("end", function () {
      var o = JSON.parse(b)
      t.equal(o._id, "npm-registry-client")
      t.equal(o["dist-tags"].latest, pkg.version)
      t.has(o.versions[pkg.version], pkg)
      t.same(o.maintainers, [ { name: 'username', email: 'i@izs.me' } ])
      t.same(o.maintainers, o.versions[pkg.version].maintainers)
      var att = o._attachments[ pkg.name + '-' + pkg.version + '.tgz' ]
      t.same(att.data, pd)
      var hash = crypto.createHash('sha1').update(pd, 'base64').digest('hex')
      t.equal(o.versions[pkg.version].dist.shasum, hash)
      res.statusCode = 201
      res.json({created:true})
    })
  })

  // not really a tarball, but doesn't matter
  var tarball = require.resolve('../package.json')
  var pd = fs.readFileSync(tarball, 'base64')
  var pkg = require('../package.json')
  client.publish(pkg, tarball, function (er, data, raw, res) {
    if (er) throw er
    t.deepEqual(data, { created: true })
    t.end()
  })
})
