var tap = require('tap')
var server = require('./fixtures/server.js')
var RC = require('../')
var client = new RC({
    cache: __dirname + '/fixtures/cache'
  , registry: 'http://localhost:' + server.port })

var userdata =
{ name: 'username',
  email: 'i@izs.me',
  _id: 'org.couchdb.user:username',
  type: 'user',
  roles: [],
  date: '2012-06-07T04:11:21.591Z' }
, password = "password"
, username = "username"
, crypto = require("crypto")
, SD = require('string_decoder').StringDecoder
, decoder = new SD

function sha (s) {
  return crypto.createHash("sha1").update(s).digest("hex")
}

tap.test("create new user account", function (t) {
  server.expect("/-/user/org.couchdb.user:username", function (req, res) {
    t.equal(req.method, "PUT")
    var b = ""
    req.on("data", function (d) {
      b += decoder.write(d)
    })

    req.on("end", function () {
      var o = JSON.parse(b)
      var salt = o.salt
      userdata.salt = salt
      userdata.password_sha = sha(password + salt)
      userdata.date = o.date
      t.deepEqual(o, userdata)

      res.statusCode = 201
      res.json({created:true})
    })
  })

  client.adduser(username, password, "i@izs.me", function (er, data, raw, res) {
    if (er) throw er
    t.deepEqual(data, { created: true })
    t.end()
  })
})
